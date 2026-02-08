#pragma once
#include <ostream>
#include "ast/Ast.h"

class AstPrinter final {
public:
    static void print(std::ostream& os, const ast::Program& program);

private:
    static void indent(std::ostream& os, int level);

    static void printProgram(std::ostream& os, const ast::Program& n, int level);
    static void printProcDef(std::ostream& os, const ast::ProcDef& n, int level);
    static void printMain(std::ostream& os, const ast::Main& n, int level);
    static void printBlock(std::ostream& os, const ast::Block& n, int level);

    static void printStmt(std::ostream& os, const ast::Stmt& n, int level);
    static void printInteraction(std::ostream& os, const ast::Interaction& n, int level);

    static void printExpr(std::ostream& os, const ast::Expr& n);
    static void printProcExpr(std::ostream& os, const ast::ProcExpr& n);
    static void printProcVar(std::ostream& os, const ast::ProcVar& n);
    static void printRaceId(std::ostream& os, const ast::RaceId& n);
};
