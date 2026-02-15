#pragma once
#include <ostream>
#include "ast/Ast.h"

class AstPrinter final {
public:
    static void print(std::ostream& os, const ast::Program& program, bool withLoc = false);

private:
    static void indent(std::ostream& os, int level);

    static void printLoc(std::ostream& os, const ast::SourceRange& loc, bool withLoc);

    static void printProgram(std::ostream& os, const ast::Program& n, int level, bool withLoc);
    static void printProcDef(std::ostream& os, const ast::ProcDef& n, int level, bool withLoc);
    static void printMain(std::ostream& os, const ast::Main& n, int level, bool withLoc);
    static void printBlock(std::ostream& os, const ast::Block& n, int level, bool withLoc);

    static void printStmt(std::ostream& os, const ast::Stmt& n, int level, bool withLoc);
    static void printInteraction(std::ostream& os, const ast::Interaction& n, int level, bool withLoc);

    static void printExpr(std::ostream& os, const ast::Expr& n, bool withLoc);
    static void printProcExpr(std::ostream& os, const ast::ProcExpr& n, bool withLoc);
    static void printProcVar(std::ostream& os, const ast::ProcVar& n, bool withLoc);
    static void printRaceId(std::ostream& os, const ast::RaceId& n, bool withLoc);
};
