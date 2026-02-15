#include "AstBuilderVisitor.h"

#include <stdexcept>

using namespace std;

std::unique_ptr<ast::Program> AstBuilderVisitor::build(RacingChoreoParser::ProgramContext* ctx) {
    return buildProgram(ctx);
}

std::string AstBuilderVisitor::idText(antlr4::tree::TerminalNode* id) {
    if (!id) return "";
    return id->getText();
}

ast::SourcePos AstBuilderVisitor::posFromToken(const antlr4::Token* tok) {
    ast::SourcePos p;
    if (!tok) return p;
    p.line = static_cast<uint32_t>(tok->getLine());
    p.col  = static_cast<uint32_t>(tok->getCharPositionInLine());
    return p;
}

ast::SourceRange AstBuilderVisitor::locFrom(antlr4::ParserRuleContext* ctx) const {
    ast::SourceRange r;
    r.file = file_;
    if (!ctx) return r;

    const antlr4::Token* start = ctx->getStart();
    const antlr4::Token* stop  = ctx->getStop();

    r.start = posFromToken(start);
    r.end   = posFromToken(stop);
    return r;
}

// ===== program =====
std::unique_ptr<ast::Program> AstBuilderVisitor::buildProgram(RacingChoreoParser::ProgramContext* ctx) {
    auto prog = std::make_unique<ast::Program>();
    prog->loc = locFrom(ctx);

    for (auto* pd : ctx->procDef()) {
        prog->procedures.push_back(buildProcDef(pd));
    }
    prog->main = buildMain(ctx->mainDef());
    return prog;
}

// ===== mainDef =====
std::unique_ptr<ast::Main> AstBuilderVisitor::buildMain(RacingChoreoParser::MainDefContext* ctx) {
    auto m = std::make_unique<ast::Main>();
    m->loc = locFrom(ctx);
    m->body = buildBlock(ctx->block());
    return m;
}

// ===== procDef =====
std::unique_ptr<ast::ProcDef> AstBuilderVisitor::buildProcDef(RacingChoreoParser::ProcDefContext* ctx) {
    auto p = std::make_unique<ast::ProcDef>();
    p->loc = locFrom(ctx);
    p->name = buildProcName(ctx->procName());
    p->params = buildProcParams(ctx->procParams());
    p->body = buildBlock(ctx->block());
    return p;
}

// ===== procParams / procArgs =====
std::vector<ast::Process> AstBuilderVisitor::buildProcParams(RacingChoreoParser::ProcParamsContext* ctx) {
    std::vector<ast::Process> res;
    for (auto* pr : ctx->process()) {
        res.push_back(buildProcess(pr));
    }
    return res;
}

std::vector<ast::Process> AstBuilderVisitor::buildProcArgs(RacingChoreoParser::ProcArgsContext* ctx) {
    std::vector<ast::Process> res;
    for (auto* pr : ctx->process()) {
        res.push_back(buildProcess(pr));
    }
    return res;
}

// ===== block =====
std::unique_ptr<ast::Block> AstBuilderVisitor::buildBlock(RacingChoreoParser::BlockContext* ctx) {
    auto b = std::make_unique<ast::Block>();
    b->loc = locFrom(ctx);
    for (auto* s : ctx->stmt()) {
        b->statements.push_back(buildStmt(s));
    }
    return b;
}

// ===== stmt =====
std::unique_ptr<ast::Stmt> AstBuilderVisitor::buildStmt(RacingChoreoParser::StmtContext* ctx) {
    if (ctx->interactionStmt()) {
        ast::InteractionStmt is = buildInteractionStmt(ctx->interactionStmt());
        auto st = ast::Stmt{ std::move(is) };
        return std::make_unique<ast::Stmt>(std::move(st));
    }
    if (ctx->callStmt()) {
        ast::CallStmt cs = buildCallStmt(ctx->callStmt());
        auto st = ast::Stmt{ std::move(cs) };
        return std::make_unique<ast::Stmt>(std::move(st));
    }
    if (ctx->ifLocalStmt()) {
        ast::IfLocalStmt s = buildIfLocalStmt(ctx->ifLocalStmt());
        auto st = ast::Stmt{ std::move(s) };
        return std::make_unique<ast::Stmt>(std::move(st));
    }
    if (ctx->ifRaceStmt()) {
        ast::IfRaceStmt s = buildIfRaceStmt(ctx->ifRaceStmt());
        auto st = ast::Stmt{ std::move(s) };
        return std::make_unique<ast::Stmt>(std::move(st));
    }

    throw std::runtime_error("Unknown stmt");
}

// ===== call / if =====
ast::CallStmt AstBuilderVisitor::buildCallStmt(RacingChoreoParser::CallStmtContext* ctx) {
    ast::CallStmt c;
    c.loc = locFrom(ctx);
    c.proc = buildProcName(ctx->procName());
    c.args = buildProcArgs(ctx->procArgs());
    return c;
}

ast::IfLocalStmt AstBuilderVisitor::buildIfLocalStmt(RacingChoreoParser::IfLocalStmtContext* ctx) {
    ast::IfLocalStmt s;
    s.loc = locFrom(ctx);
    s.condition = buildProcExpr(ctx->procExpr());
    s.thenBlock = buildBlock(ctx->block(0));
    s.elseBlock = buildBlock(ctx->block(1));
    return s;
}

ast::IfRaceStmt AstBuilderVisitor::buildIfRaceStmt(RacingChoreoParser::IfRaceStmtContext* ctx) {
    ast::IfRaceStmt s;
    s.loc = locFrom(ctx);
    s.condition = buildRaceId(ctx->raceId());
    s.thenBlock = buildBlock(ctx->block(0));
    s.elseBlock = buildBlock(ctx->block(1));
    return s;
}

// ===== interactionStmt / interaction =====
ast::InteractionStmt AstBuilderVisitor::buildInteractionStmt(RacingChoreoParser::InteractionStmtContext* ctx) {
    ast::InteractionStmt s;
    s.loc = locFrom(ctx);
    s.interaction = buildInteraction(ctx->interaction());
    return s;
}

ast::Interaction AstBuilderVisitor::buildInteraction(RacingChoreoParser::InteractionContext* ctx) {
    if (ctx->comm()) return ast::Interaction{ buildComm(ctx->comm()) };
    if (ctx->select()) return ast::Interaction{ buildSelect(ctx->select()) };
    if (ctx->assign()) return ast::Interaction{ buildAssign(ctx->assign()) };
    if (ctx->race()) return ast::Interaction{ buildRace(ctx->race()) };
    if (ctx->discharge()) return ast::Interaction{ buildDischarge(ctx->discharge()) };
    throw std::runtime_error("Unknown interaction");
}

// ===== concrete interactions =====
ast::Comm AstBuilderVisitor::buildComm(RacingChoreoParser::CommContext* ctx) {
    ast::Comm c;
    c.loc = locFrom(ctx);
    c.from = buildProcExpr(ctx->procExpr());
    c.to = buildProcVar(ctx->procVar());
    return c;
}

ast::Select AstBuilderVisitor::buildSelect(RacingChoreoParser::SelectContext* ctx) {
    ast::Select s;
    s.loc = locFrom(ctx);
    s.from = buildProcess(ctx->process(0));
    s.to = buildProcess(ctx->process(1));
    s.label = buildLabel(ctx->label());
    return s;
}

ast::Assign AstBuilderVisitor::buildAssign(RacingChoreoParser::AssignContext* ctx) {
    ast::Assign a;
    a.loc = locFrom(ctx);
    a.target = buildProcVar(ctx->procVar());
    a.value = buildExpr(ctx->expr());
    return a;
}

ast::Race AstBuilderVisitor::buildRace(RacingChoreoParser::RaceContext* ctx) {
    ast::Race r;
    r.loc = locFrom(ctx);
    r.id = buildRaceId(ctx->raceId());
    r.left = buildProcExpr(ctx->procExpr(0));
    r.right = buildProcExpr(ctx->procExpr(1));
    r.target = buildProcVar(ctx->procVar());
    return r;
}

ast::Discharge AstBuilderVisitor::buildDischarge(RacingChoreoParser::DischargeContext* ctx) {
    ast::Discharge d;
    d.loc = locFrom(ctx);
    d.id = buildRaceId(ctx->raceId());
    d.source = buildProcess(ctx->process());
    d.target = buildProcVar(ctx->procVar());
    return d;
}

// ===== procExpr / procVar / expr / raceId =====
ast::ProcExpr AstBuilderVisitor::buildProcExpr(RacingChoreoParser::ProcExprContext* ctx) {
    ast::ProcExpr pe;
    pe.loc = locFrom(ctx);
    pe.process = buildProcess(ctx->process());
    pe.expr = buildExpr(ctx->expr());
    return pe;
}

ast::ProcVar AstBuilderVisitor::buildProcVar(RacingChoreoParser::ProcVarContext* ctx) {
    ast::ProcVar pv;
    pv.loc = locFrom(ctx);
    pv.process = buildProcess(ctx->process());
    pv.var = buildVar(ctx->var());
    return pv;
}

ast::Expr AstBuilderVisitor::buildExpr(RacingChoreoParser::ExprContext* ctx) {
    if (ctx->var()) {
        ast::ExprVar v{ buildVar(ctx->var()) };
        v.loc = locFrom(ctx);
        return ast::Expr{ v };
    }

    auto* vctx = ctx->value();
    if (!vctx) throw std::runtime_error("Expr without var/value");

    ast::Value val;
    val.loc = locFrom(ctx);

    if (vctx->INT()) {
        val.kind = ast::Value::Kind::Int;
        val.intValue = std::stoi(vctx->INT()->getText());
    } else if (vctx->TRUE()) {
        val.kind = ast::Value::Kind::Bool;
        val.boolValue = true;
    } else if (vctx->FALSE()) {
        val.kind = ast::Value::Kind::Bool;
        val.boolValue = false;
    } else {
        throw std::runtime_error("Unknown value");
    }
    return ast::Expr{ val };
}

ast::RaceId AstBuilderVisitor::buildRaceId(RacingChoreoParser::RaceIdContext* ctx) {
    ast::RaceId id;
    id.loc = locFrom(ctx);
    id.process = buildProcess(ctx->process());
    id.key = ctx->raceKey()->getText();
    return id;
}

// ===== leaves =====
ast::Process AstBuilderVisitor::buildProcess(RacingChoreoParser::ProcessContext* ctx) {
    return idText(ctx->ID());
}

ast::Var AstBuilderVisitor::buildVar(RacingChoreoParser::VarContext* ctx) {
    return idText(ctx->ID());
}

ast::Label AstBuilderVisitor::buildLabel(RacingChoreoParser::LabelContext* ctx) {
    return idText(ctx->ID());
}

ast::ProcName AstBuilderVisitor::buildProcName(RacingChoreoParser::ProcNameContext* ctx) {
    return idText(ctx->ID());
}
