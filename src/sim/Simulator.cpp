#include "sim/Simulator.h"

#include <unordered_map>
#include <sstream>
#include <variant>
#include <vector>
#include <random>

#include "runtime/RuntimeError.h"
#include "runtime/Value.h"
#include "runtime/Store.h"
#include "runtime/Trace.h"
#include "runtime/RaceMemory.h"

namespace sim {

namespace {

// loc finta per init
static ast::SourceRange initLoc() {
    ast::SourceRange r;
    r.file = "<init>";
    r.start.line = 0;
    r.start.col = 0;
    r.end = r.start;
    return r;
}

struct ExecCtx {
    const SimOptions& opt;
    runtime::Store store;
    runtime::RaceMemory races;
    runtime::Trace trace;

    uint64_t steps = 0;
    uint64_t callDepth = 0;

    std::mt19937_64 rng;

    explicit ExecCtx(const SimOptions& o)
        : opt(o), rng(o.seed) {}
};

struct BlockFrame {
    const ast::Block* block = nullptr;
    size_t ip = 0;
    std::unordered_map<std::string, std::string> subst;

    // empty for main
    std::string procName;

    // call-site loc (for ret trace)
    ast::SourceRange callLoc;
};

// -------------------- helpers --------------------
static runtime::Value toRuntimeValue(const ast::Value& v) {
    if (v.kind == ast::Value::Kind::Int) return runtime::Value::makeInt(v.intValue);
    return runtime::Value::makeBool(v.boolValue);
}

static std::string processSubst(const std::string& p,
                                const std::unordered_map<std::string, std::string>& subst) {
    auto it = subst.find(p);
    if (it == subst.end()) return p;
    return it->second;
}

static void checkStepLimit(ExecCtx& ctx, const ast::SourceRange& loc) {
    ctx.steps++;
    if (ctx.steps > ctx.opt.maxSteps) {
        throw runtime::RuntimeError(loc, "max steps exceeded");
    }
}

static void checkCallDepth(ExecCtx& ctx, const ast::SourceRange& loc) {
    if (ctx.callDepth >= ctx.opt.maxCallDepth) {
        throw runtime::RuntimeError(loc, "max call depth exceeded");
    }
}

static void pushTrace(ExecCtx& ctx,
                      const std::string& kind,
                      const std::string& msg,
                      const ast::SourceRange& loc) {
    if (!ctx.opt.trace) return;
    runtime::TraceEvent ev;
    ev.kind = kind;
    ev.message = msg;
    ev.loc = loc;
    ctx.trace.push_back(std::move(ev));
}

static std::string exprToString(const ast::Expr& e) {
    return std::visit([&](auto&& node) -> std::string {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ast::ExprVar>) return node.name;
        if constexpr (std::is_same_v<T, ast::Value>) {
            if (node.kind == ast::Value::Kind::Int) return std::to_string(node.intValue);
            return node.boolValue ? "true" : "false";
        }
        return "<expr>";
    }, e);
}

static std::string procExprToString(const ast::ProcExpr& pe,
                                    const std::unordered_map<std::string, std::string>& subst) {
    const std::string pEff = processSubst(pe.process, subst);
    return pEff + "." + exprToString(pe.expr);
}

static std::string procVarToString(const ast::ProcVar& pv,
                                   const std::unordered_map<std::string, std::string>& subst) {
    const std::string pEff = processSubst(pv.process, subst);
    return pEff + "." + pv.var;
}

static std::unordered_map<std::string, std::string>
composeSubst(const std::unordered_map<std::string, std::string>& outer,
             const std::unordered_map<std::string, std::string>& inner) {
    std::unordered_map<std::string, std::string> res = outer;

    for (const auto& kv : inner) {
        const std::string& formal = kv.first;
        const std::string& actual = kv.second;

        std::string resolved = actual;
        auto it = outer.find(actual);
        if (it != outer.end()) resolved = it->second;

        res[formal] = resolved;
    }

    return res;
}

// Σ(p,e) ↓ v
static runtime::Value evalExpr(ExecCtx& ctx,
                               const std::string& process,
                               const ast::Expr& expr,
                               const std::unordered_map<std::string, std::string>& subst,
                               const ast::SourceRange& errLoc) {
    const std::string pEff = processSubst(process, subst);

    return std::visit([&](auto&& node) -> runtime::Value {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ast::Value>) {
            return toRuntimeValue(node);
        } else if constexpr (std::is_same_v<T, ast::ExprVar>) {
            auto ov = ctx.store.tryGet(pEff, node.name);
            if (!ov.has_value()) {
                std::ostringstream ss;
                ss << "uninitialized variable '" << pEff << "." << node.name << "'";
                const ast::SourceRange loc = (node.loc.file.empty() ? errLoc : node.loc);
                throw runtime::RuntimeError(loc, ss.str());
            }
            return *ov;
        }

        throw runtime::RuntimeError(errLoc, "unknown expression kind");
    }, expr);
}

static runtime::Value evalProcExpr(ExecCtx& ctx,
                                  const ast::ProcExpr& pe,
                                  const std::unordered_map<std::string, std::string>& subst) {
    return evalExpr(ctx, pe.process, pe.expr, subst, pe.loc);
}

// -------------------- concrete actions --------------------
static void execAssign(ExecCtx& ctx,
                       const ast::Assign& a,
                       const std::unordered_map<std::string, std::string>& subst) {
    const std::string targetProcEff = processSubst(a.target.process, subst);
    runtime::Value v = evalExpr(ctx, a.target.process, a.value, subst, a.loc);
    ctx.store.set(targetProcEff, a.target.var, v);

    std::ostringstream ss;
    ss << procVarToString(a.target, subst) << " = " << v.toString();
    pushTrace(ctx, "asg", ss.str(), a.loc);
}

static void execComm(ExecCtx& ctx,
                     const ast::Comm& c,
                     const std::unordered_map<std::string, std::string>& subst) {
    const std::string toProcEff = processSubst(c.to.process, subst);
    runtime::Value v = evalProcExpr(ctx, c.from, subst);
    ctx.store.set(toProcEff, c.to.var, v);

    std::ostringstream ss;
    ss << procExprToString(c.from, subst) << " = " << v.toString()
       << " -> " << procVarToString(c.to, subst);
    pushTrace(ctx, "com", ss.str(), c.loc);
}

static void execSelect(ExecCtx& ctx,
                       const ast::Select& s,
                       const std::unordered_map<std::string, std::string>& subst) {
    const std::string fromEff = processSubst(s.from, subst);
    const std::string toEff   = processSubst(s.to, subst);

    std::ostringstream ss;
    ss << fromEff << " -> " << toEff << " [" << s.label << "]";
    pushTrace(ctx, "sel", ss.str(), s.loc);
}

static bool requireBool(const runtime::Value& v, const ast::SourceRange& loc) {
    if (v.kind != runtime::Value::Kind::Bool) {
        throw runtime::RuntimeError(loc, "condition is not a boolean");
    }
    return v.boolValue;
}

static std::unordered_map<std::string, const ast::ProcDef*>
buildProcTable(const ast::Program& program) {
    std::unordered_map<std::string, const ast::ProcDef*> table;
    for (const auto& p : program.procedures) table[p->name] = p.get();
    return table;
}

static std::unordered_map<std::string, std::string>
buildCallSubst(const ast::ProcDef& def,
               const ast::CallStmt& call,
               const std::unordered_map<std::string, std::string>& callerSubst) {
    if (def.params.size() != call.args.size()) {
        std::ostringstream ss;
        ss << "procedure '" << def.name << "' arity mismatch at runtime";
        throw runtime::RuntimeError(call.loc, ss.str());
    }

    std::unordered_map<std::string, std::string> inner;
    for (size_t i = 0; i < def.params.size(); ++i) {
        const std::string& formal = def.params[i];
        const std::string& actual = call.args[i];
        inner[formal] = processSubst(actual, callerSubst);
    }

    return inner;
}

static runtime::RaceWinnerSide decideRaceWinnerSide(ExecCtx& ctx, const ast::SourceRange& loc) {
    switch (ctx.opt.racePolicy) {
    case RacePolicy::Left:  return runtime::RaceWinnerSide::Left;
    case RacePolicy::Right: return runtime::RaceWinnerSide::Right;
    case RacePolicy::Random: {
        std::uniform_int_distribution<int> dist(0, 1);
        return (dist(ctx.rng) == 0) ? runtime::RaceWinnerSide::Left
                                   : runtime::RaceWinnerSide::Right;
    }
    default:
        throw runtime::RuntimeError(loc, "invalid race policy");
    }
}

static runtime::RaceKey toRaceKey(const ast::RaceId& id,
                                  const std::unordered_map<std::string, std::string>& subst) {
    runtime::RaceKey k;
    k.process = processSubst(id.process, subst);
    k.key = id.key;
    return k;
}

static void execRace(ExecCtx& ctx,
                     const ast::Race& r,
                     const std::unordered_map<std::string, std::string>& subst) {
    runtime::RaceKey key = toRaceKey(r.id, subst);

    if (ctx.races.contains(key)) {
        std::ostringstream ss;
        ss << "race '" << key.process << "[" << key.key << "]' already resolved";
        throw runtime::RuntimeError(r.loc, ss.str());
    }

    runtime::Value vL = evalProcExpr(ctx, r.left, subst);
    runtime::Value vR = evalProcExpr(ctx, r.right, subst);

    const std::string leftProcEff  = processSubst(r.left.process, subst);
    const std::string rightProcEff = processSubst(r.right.process, subst);

    runtime::RaceWinnerSide side = decideRaceWinnerSide(ctx, r.loc);

    runtime::RaceEntry entry;
    entry.leftProc = leftProcEff;
    entry.rightProc = rightProcEff;

    if (side == runtime::RaceWinnerSide::Left) {
        entry.winnerSide = runtime::RaceWinnerSide::Left;
        entry.winnerProc = leftProcEff;
        entry.loserProc = rightProcEff;
        entry.vWinner = vL;
        entry.vLoser = vR;
    } else {
        entry.winnerSide = runtime::RaceWinnerSide::Right;
        entry.winnerProc = rightProcEff;
        entry.loserProc = leftProcEff;
        entry.vWinner = vR;
        entry.vLoser = vL;
    }

    const std::string targetProcEff = processSubst(r.target.process, subst);
    ctx.store.set(targetProcEff, r.target.var, entry.vWinner);

    ctx.races.put(key, entry);

    const runtime::RaceEntry* saved = ctx.races.get(key);
    std::ostringstream ss;
    ss << key.process << "[" << key.key << "] winner=" << saved->winnerProc
       << " loser=" << saved->loserProc
       << " write " << targetProcEff << "." << r.target.var << "=" << saved->vWinner.toString();
    pushTrace(ctx, "race", ss.str(), r.loc);
}

static void execIfRace(ExecCtx& ctx,
                       const ast::IfRaceStmt& s,
                       const std::unordered_map<std::string, std::string>& subst,
                       const ast::Block*& chosenOut,
                       std::string& traceMsgOut) {
    runtime::RaceKey key = toRaceKey(s.condition, subst);
    const runtime::RaceEntry* entry = ctx.races.get(key);
    if (!entry) {
        std::ostringstream ss;
        ss << "race '" << key.process << "[" << key.key << "]' not resolved";
        throw runtime::RuntimeError(s.loc, ss.str());
    }

    const bool cond = (entry->winnerSide == runtime::RaceWinnerSide::Left);
    chosenOut = cond ? s.thenBlock.get() : s.elseBlock.get();

    std::ostringstream ss;
    ss << key.process << "[" << key.key << "] winner=" << entry->winnerProc
       << " -> " << (cond ? "then" : "else");
    traceMsgOut = ss.str();
}

static void execDischarge(ExecCtx& ctx,
                          const ast::Discharge& d,
                          const std::unordered_map<std::string, std::string>& subst) {
    runtime::RaceKey key = toRaceKey(d.id, subst);

    runtime::RaceEntry* entry = ctx.races.getMut(key);
    if (!entry) {
        std::ostringstream ss;
        ss << "race '" << key.process << "[" << key.key << "]' not resolved";
        throw runtime::RuntimeError(d.loc, ss.str());
    }

    const std::string loserExpected = entry->loserProc;
    const std::string ellEff = processSubst(d.source, subst);

    if (ellEff != loserExpected) {
        std::ostringstream ss;
        ss << "discharge expects loser '" << loserExpected << "', got '" << ellEff << "'";
        throw runtime::RuntimeError(d.loc, ss.str());
    }

    if (entry->discharged) {
        std::ostringstream ss;
        ss << "race '" << key.process << "[" << key.key << "]' already discharged";
        throw runtime::RuntimeError(d.loc, ss.str());
    }

    const std::string targetProcEff = processSubst(d.target.process, subst);
    ctx.store.set(targetProcEff, d.target.var, entry->vLoser);

    entry->discharged = true;

    std::ostringstream ss;
    ss << key.process << "[" << key.key << "] loser=" << ellEff
       << " write " << targetProcEff << "." << d.target.var << "=" << entry->vLoser.toString();
    pushTrace(ctx, "dis", ss.str(), d.loc);
}

} // namespace

SimulationResult Simulator::run(const ast::Program& program, const SimOptions& opt) {
    SimulationResult res;
    res.ok = false;

    ExecCtx ctx(opt);

    try {
        // ---- APPLY INIT (da --init ...) ----
        {
            const ast::SourceRange loc = initLoc();
            for (const auto& b : opt.init) {
                ctx.store.set(b.process, b.var, b.value);

                std::ostringstream ss;
                ss << b.process << "." << b.var << " = " << b.value.toString();
                pushTrace(ctx, "init", ss.str(), loc);
            }
        }

        const auto procTable = buildProcTable(program);

        std::vector<BlockFrame> stack;
        stack.push_back(BlockFrame{ program.main->body.get(), 0, {}, "", {} });

        while (!stack.empty()) {
            BlockFrame& fr = stack.back();

            if (fr.ip >= fr.block->statements.size()) {
                if (!fr.procName.empty()) {
                    ctx.callDepth--;

                    const ast::SourceRange& loc =
                        (!fr.callLoc.file.empty() ? fr.callLoc : program.loc);

                    pushTrace(ctx, "ret", fr.procName, loc);
                }
                stack.pop_back();
                continue;
            }

            const ast::Stmt& st = *fr.block->statements[fr.ip];

            std::visit([&](auto&& node) {
                using T = std::decay_t<decltype(node)>;

                if constexpr (std::is_same_v<T, ast::InteractionStmt>) {
                    checkStepLimit(ctx, node.loc);
                    fr.ip++;

                    std::visit([&](auto&& inNode) {
                        using I = std::decay_t<decltype(inNode)>;

                        if constexpr (std::is_same_v<I, ast::Assign>) {
                            execAssign(ctx, inNode, fr.subst);
                        } else if constexpr (std::is_same_v<I, ast::Comm>) {
                            execComm(ctx, inNode, fr.subst);
                        } else if constexpr (std::is_same_v<I, ast::Select>) {
                            execSelect(ctx, inNode, fr.subst);
                        } else if constexpr (std::is_same_v<I, ast::Race>) {
                            execRace(ctx, inNode, fr.subst);
                        } else if constexpr (std::is_same_v<I, ast::Discharge>) {
                            execDischarge(ctx, inNode, fr.subst);
                        }
                    }, node.interaction);

                } else if constexpr (std::is_same_v<T, ast::IfLocalStmt>) {
                    checkStepLimit(ctx, node.loc);

                    runtime::Value condV = evalProcExpr(ctx, node.condition, fr.subst);
                    bool cond = requireBool(condV, node.condition.loc);

                    std::ostringstream ss;
                    ss << "cond=" << (cond ? "true" : "false")
                       << " @ " << procExprToString(node.condition, fr.subst)
                       << " -> " << (cond ? "then" : "else");
                    pushTrace(ctx, "if", ss.str(), node.loc);

                    fr.ip++;
                    const ast::Block* chosen = cond ? node.thenBlock.get() : node.elseBlock.get();
                    stack.push_back(BlockFrame{ chosen, 0, fr.subst, "", {} });

                } else if constexpr (std::is_same_v<T, ast::IfRaceStmt>) {
                    checkStepLimit(ctx, node.loc);

                    const ast::Block* chosen = nullptr;
                    std::string msg;
                    execIfRace(ctx, node, fr.subst, chosen, msg);
                    pushTrace(ctx, "ifRace", msg, node.loc);

                    fr.ip++;
                    stack.push_back(BlockFrame{ chosen, 0, fr.subst, "", {} });

                } else if constexpr (std::is_same_v<T, ast::CallStmt>) {
                    checkStepLimit(ctx, node.loc);

                    auto it = procTable.find(node.proc);
                    if (it == procTable.end()) {
                        std::ostringstream ss;
                        ss << "call to undefined procedure '" << node.proc << "'";
                        throw runtime::RuntimeError(node.loc, ss.str());
                    }
                    const ast::ProcDef* def = it->second;

                    checkCallDepth(ctx, node.loc);
                    ctx.callDepth++;

                    {
                        std::ostringstream ss;
                        ss << node.proc << "(";
                        for (size_t i = 0; i < node.args.size(); ++i) {
                            if (i) ss << ",";
                            ss << processSubst(node.args[i], fr.subst);
                        }
                        ss << ")";
                        pushTrace(ctx, "call", ss.str(), node.loc);
                    }

                    auto inner = buildCallSubst(*def, node, fr.subst);
                    auto composed = composeSubst(fr.subst, inner);

                    fr.ip++;
                    stack.push_back(BlockFrame{ def->body.get(), 0, composed, node.proc, node.loc });

                } else {
                    throw runtime::RuntimeError(program.loc, "unknown statement kind");
                }

            }, st);
        }

        res.ok = true;
        res.store = std::move(ctx.store);
        res.races = std::move(ctx.races);
        res.trace = std::move(ctx.trace);
        return res;

    } catch (const runtime::RuntimeError& re) {
        RuntimeErrorInfo e;
        e.file = re.loc().file;
        e.line = re.loc().start.line;
        e.col  = re.loc().start.col;
        e.message = re.what();
        res.runtimeErrors.push_back(std::move(e));

        res.store = std::move(ctx.store);
        res.races = std::move(ctx.races);
        res.trace = std::move(ctx.trace);
        res.ok = false;
        return res;

    } catch (const std::exception& ex) {
        RuntimeErrorInfo e;
        e.file = "<internal>";
        e.line = 0;
        e.col  = 0;
        e.message = ex.what();
        res.runtimeErrors.push_back(std::move(e));

        res.store = std::move(ctx.store);
        res.races = std::move(ctx.races);
        res.trace = std::move(ctx.trace);
        res.ok = false;
        return res;
    }
}

} // namespace sim