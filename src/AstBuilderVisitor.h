#pragma once
#include <memory>
#include <string>
#include <vector>

#include "ast/Ast.h"

// ANTLR generated
#include "RacingChoreoBaseVisitor.h"
#include "RacingChoreoParser.h"

class AstBuilderVisitor final : public RacingChoreoBaseVisitor {
public:
    explicit AstBuilderVisitor(std::string file = "<unknown>")
        : file_(std::move(file)) {}

    // entry point comodo: costruisce l'AST da un parse tree program()
    std::unique_ptr<ast::Program> build(RacingChoreoParser::ProgramContext* ctx);

private:
    std::string file_;

    static std::string idText(antlr4::tree::TerminalNode* id);

    // location helpers
    ast::SourceRange locFrom(antlr4::ParserRuleContext* ctx) const;
    static ast::SourcePos posFromToken(const antlr4::Token* tok);

    // ---- build functions (mappano 1-1 le parser rules) ----
    std::unique_ptr<ast::Program> buildProgram(RacingChoreoParser::ProgramContext* ctx);
    std::unique_ptr<ast::Main>    buildMain(RacingChoreoParser::MainDefContext* ctx);
    std::unique_ptr<ast::ProcDef> buildProcDef(RacingChoreoParser::ProcDefContext* ctx);
    std::unique_ptr<ast::Block>   buildBlock(RacingChoreoParser::BlockContext* ctx);

    std::unique_ptr<ast::Stmt> buildStmt(RacingChoreoParser::StmtContext* ctx);

    ast::CallStmt     buildCallStmt(RacingChoreoParser::CallStmtContext* ctx);
    ast::IfLocalStmt  buildIfLocalStmt(RacingChoreoParser::IfLocalStmtContext* ctx);
    ast::IfRaceStmt   buildIfRaceStmt(RacingChoreoParser::IfRaceStmtContext* ctx);
    ast::InteractionStmt buildInteractionStmt(RacingChoreoParser::InteractionStmtContext* ctx);

    ast::Interaction buildInteraction(RacingChoreoParser::InteractionContext* ctx);

    ast::Comm      buildComm(RacingChoreoParser::CommContext* ctx);
    ast::Select    buildSelect(RacingChoreoParser::SelectContext* ctx);
    ast::Assign    buildAssign(RacingChoreoParser::AssignContext* ctx);
    ast::Race      buildRace(RacingChoreoParser::RaceContext* ctx);
    ast::Discharge buildDischarge(RacingChoreoParser::DischargeContext* ctx);

    ast::ProcExpr buildProcExpr(RacingChoreoParser::ProcExprContext* ctx);
    ast::ProcVar  buildProcVar(RacingChoreoParser::ProcVarContext* ctx);
    ast::Expr     buildExpr(RacingChoreoParser::ExprContext* ctx);
    ast::RaceId   buildRaceId(RacingChoreoParser::RaceIdContext* ctx);

    std::vector<ast::Process> buildProcParams(RacingChoreoParser::ProcParamsContext* ctx);
    std::vector<ast::Process> buildProcArgs(RacingChoreoParser::ProcArgsContext* ctx);

    // leaf rules
    ast::Process  buildProcess(RacingChoreoParser::ProcessContext* ctx);
    ast::Var      buildVar(RacingChoreoParser::VarContext* ctx);
    ast::Label    buildLabel(RacingChoreoParser::LabelContext* ctx);
    ast::ProcName buildProcName(RacingChoreoParser::ProcNameContext* ctx);
};
