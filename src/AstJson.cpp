#include "AstJson.h"
#include "Json.h"

#include <sstream>
#include <variant>
#include <memory>

using std::string;

namespace astjson {

// ---------- helpers: serialize leaf/nodes to string using json::Writer ----------
static string serializeExpr(const ast::Expr& e);
static string serializeProcExpr(const ast::ProcExpr& e);
static string serializeProcVar(const ast::ProcVar& v);
static string serializeRaceId(const ast::RaceId& id);
static string serializeInteraction(const ast::Interaction& in);
static string serializeStmt(const ast::Stmt& st);
static string serializeBlock(const ast::Block& b);
static string serializeMain(const ast::Main& m);
static string serializeProcDef(const ast::ProcDef& p);

static string serializeLoc(const ast::SourceRange& loc) {
    std::ostringstream ss;
    json::Writer w(ss, 2);
    w.beginObject();
    w.keyString("file", loc.file);
    w.keyInt("line", static_cast<int>(loc.start.line));
    w.keyInt("col", static_cast<int>(loc.start.col));
    w.endObject();
    return ss.str();
}

static string serializeExpr(const ast::Expr& e) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ast::ExprVar>) {
            w.keyString("kind", "Var");
            w.keyString("name", node.name);
        } else if constexpr (std::is_same_v<T, ast::Value>) {
            w.keyString("kind", "Value");
            if (node.kind == ast::Value::Kind::Int) {
                w.keyString("type", "int");
                w.keyInt("value", node.intValue);
            } else {
                w.keyString("type", "bool");
                w.keyBool("value", node.boolValue);
            }
        }
    }, e);
    // Expr non ha loc nel tuo AST: quindi niente "loc" qui
    w.endObject();
    return ss.str();
}

static string serializeProcExpr(const ast::ProcExpr& e) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    w.keyString("kind", "ProcExpr");
    w.keyString("process", e.process);
    w.keyRaw("expr", serializeExpr(e.expr));
    w.keyRaw("loc", serializeLoc(e.loc));
    w.endObject();

    return ss.str();
}

static string serializeProcVar(const ast::ProcVar& v) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    w.keyString("kind", "ProcVar");
    w.keyString("process", v.process);
    w.keyString("var", v.var);
    w.keyRaw("loc", serializeLoc(v.loc));
    w.endObject();

    return ss.str();
}

static string serializeRaceId(const ast::RaceId& id) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    w.keyString("kind", "RaceId");
    w.keyString("process", id.process);
    w.keyString("key", id.key);
    w.keyRaw("loc", serializeLoc(id.loc));
    w.endObject();

    return ss.str();
}

static string serializeInteraction(const ast::Interaction& in) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ast::Comm>) {
            w.keyString("kind", "Comm");
            w.keyRaw("from", serializeProcExpr(node.from));
            w.keyRaw("to", serializeProcVar(node.to));
            w.keyRaw("loc", serializeLoc(node.loc));
        } else if constexpr (std::is_same_v<T, ast::Select>) {
            w.keyString("kind", "Select");
            w.keyString("from", node.from);
            w.keyString("to", node.to);
            w.keyString("label", node.label);
            w.keyRaw("loc", serializeLoc(node.loc));
        } else if constexpr (std::is_same_v<T, ast::Assign>) {
            w.keyString("kind", "Assign");
            w.keyRaw("target", serializeProcVar(node.target));
            w.keyRaw("value", serializeExpr(node.value));
            w.keyRaw("loc", serializeLoc(node.loc));
        } else if constexpr (std::is_same_v<T, ast::Race>) {
            w.keyString("kind", "Race");
            w.keyRaw("id", serializeRaceId(node.id));
            w.keyRaw("left", serializeProcExpr(node.left));
            w.keyRaw("right", serializeProcExpr(node.right));
            w.keyRaw("target", serializeProcVar(node.target));
            w.keyRaw("loc", serializeLoc(node.loc));
        } else if constexpr (std::is_same_v<T, ast::Discharge>) {
            w.keyString("kind", "Discharge");
            w.keyRaw("id", serializeRaceId(node.id));
            w.keyString("source", node.source);
            w.keyRaw("target", serializeProcVar(node.target));
            w.keyRaw("loc", serializeLoc(node.loc));
        }
    }, in);
    w.endObject();

    return ss.str();
}

static string serializeStmt(const ast::Stmt& st) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ast::InteractionStmt>) {
            w.keyString("kind", "InteractionStmt");
            w.keyRaw("interaction", serializeInteraction(node.interaction));
            w.keyRaw("loc", serializeLoc(node.loc));
        } else if constexpr (std::is_same_v<T, ast::CallStmt>) {
            w.keyString("kind", "CallStmt");
            w.keyString("proc", node.proc);

            w.beginArray("args");
            for (const auto& a : node.args) w.elementString(a);
            w.endArray();

            w.keyRaw("loc", serializeLoc(node.loc));
        } else if constexpr (std::is_same_v<T, ast::IfLocalStmt>) {
            w.keyString("kind", "IfLocalStmt");
            w.keyRaw("condition", serializeProcExpr(node.condition));
            w.keyRaw("then", serializeBlock(*node.thenBlock));
            w.keyRaw("else", serializeBlock(*node.elseBlock));
            w.keyRaw("loc", serializeLoc(node.loc));
        } else if constexpr (std::is_same_v<T, ast::IfRaceStmt>) {
            w.keyString("kind", "IfRaceStmt");
            w.keyRaw("condition", serializeRaceId(node.condition));
            w.keyRaw("then", serializeBlock(*node.thenBlock));
            w.keyRaw("else", serializeBlock(*node.elseBlock));
            w.keyRaw("loc", serializeLoc(node.loc));
        }
    }, st);
    w.endObject();

    return ss.str();
}

static string serializeBlock(const ast::Block& b) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    w.keyString("kind", "Block");

    w.beginArray("statements");
    for (const auto& st : b.statements) {
        w.elementObjectBegin();
        w.keyRaw("node", serializeStmt(*st));
        w.elementObjectEnd();
    }
    w.endArray();

    w.keyRaw("loc", serializeLoc(b.loc));
    w.endObject();

    return ss.str();
}

static string serializeProcDef(const ast::ProcDef& p) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    w.keyString("kind", "ProcDef");
    w.keyString("name", p.name);

    w.beginArray("params");
    for (const auto& x : p.params) w.elementString(x);
    w.endArray();

    w.keyRaw("body", serializeBlock(*p.body));
    w.keyRaw("loc", serializeLoc(p.loc));
    w.endObject();

    return ss.str();
}

static string serializeMain(const ast::Main& m) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    w.keyString("kind", "Main");
    w.keyRaw("body", serializeBlock(*m.body));
    w.keyRaw("loc", serializeLoc(m.loc));
    w.endObject();

    return ss.str();
}

// ---------- public entry ----------
std::string serialize(const ast::Program& program) {
    std::ostringstream ss;
    json::Writer w(ss, 2);

    w.beginObject();
    w.keyString("kind", "Program");

    w.beginArray("procedures");
    for (const auto& p : program.procedures) {
        w.elementObjectBegin();
        w.keyRaw("node", serializeProcDef(*p));
        w.elementObjectEnd();
    }
    w.endArray();

    w.keyRaw("main", serializeMain(*program.main));
    w.keyRaw("loc", serializeLoc(program.loc));
    w.endObject();

    return ss.str();
}

} // namespace astjson
