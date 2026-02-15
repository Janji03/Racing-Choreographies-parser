#include "AstPrinter.h"

#include <variant>

using namespace std;

void AstPrinter::print(std::ostream& os, const ast::Program& program, bool withLoc) {
    printProgram(os, program, 0, withLoc);
}

void AstPrinter::indent(std::ostream& os, int level) {
    for (int i = 0; i < level; ++i) os << "  ";
}

void AstPrinter::printLoc(std::ostream& os, const ast::SourceRange& loc, bool withLoc) {
    if (!withLoc) return;
    os << " @" << loc.file << ":" << loc.start.line << ":" << loc.start.col;
}

void AstPrinter::printProgram(std::ostream& os, const ast::Program& n, int level, bool withLoc) {
    indent(os, level);
    os << "Program";
    printLoc(os, n.loc, withLoc);
    os << "\n";

    indent(os, level + 1);
    os << "Procedures (" << n.procedures.size() << ")\n";
    for (const auto& p : n.procedures) {
        printProcDef(os, *p, level + 2, withLoc);
    }

    indent(os, level + 1);
    os << "Main\n";
    printMain(os, *n.main, level + 2, withLoc);
}

void AstPrinter::printProcDef(std::ostream& os, const ast::ProcDef& n, int level, bool withLoc) {
    indent(os, level);
    os << "ProcDef " << n.name << "(";
    for (size_t i = 0; i < n.params.size(); ++i) {
        if (i) os << ",";
        os << n.params[i];
    }
    os << ")";
    printLoc(os, n.loc, withLoc);
    os << "\n";
    printBlock(os, *n.body, level + 1, withLoc);
}

void AstPrinter::printMain(std::ostream& os, const ast::Main& n, int level, bool withLoc) {
    (void)n;
    printBlock(os, *n.body, level, withLoc);
}

void AstPrinter::printBlock(std::ostream& os, const ast::Block& n, int level, bool withLoc) {
    indent(os, level);
    os << "Block (" << n.statements.size() << " stmt)";
    printLoc(os, n.loc, withLoc);
    os << "\n";
    for (const auto& s : n.statements) {
        printStmt(os, *s, level + 1, withLoc);
    }
}

void AstPrinter::printStmt(std::ostream& os, const ast::Stmt& n, int level, bool withLoc) {
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ast::InteractionStmt>) {
            indent(os, level);
            os << "InteractionStmt";
            printLoc(os, node.loc, withLoc);
            os << "\n";
            printInteraction(os, node.interaction, level + 1, withLoc);
        } else if constexpr (std::is_same_v<T, ast::CallStmt>) {
            indent(os, level);
            os << "Call " << node.proc << "(";
            for (size_t i = 0; i < node.args.size(); ++i) {
                if (i) os << ",";
                os << node.args[i];
            }
            os << ")";
            printLoc(os, node.loc, withLoc);
            os << "\n";
        } else if constexpr (std::is_same_v<T, ast::IfLocalStmt>) {
            indent(os, level);
            os << "IfLocal (";
            printProcExpr(os, node.condition, withLoc);
            os << ")";
            printLoc(os, node.loc, withLoc);
            os << "\n";
            indent(os, level);
            os << "Then:\n";
            printBlock(os, *node.thenBlock, level + 1, withLoc);
            indent(os, level);
            os << "Else:\n";
            printBlock(os, *node.elseBlock, level + 1, withLoc);
        } else if constexpr (std::is_same_v<T, ast::IfRaceStmt>) {
            indent(os, level);
            os << "IfRace (";
            printRaceId(os, node.condition, withLoc);
            os << ")";
            printLoc(os, node.loc, withLoc);
            os << "\n";
            indent(os, level);
            os << "Then:\n";
            printBlock(os, *node.thenBlock, level + 1, withLoc);
            indent(os, level);
            os << "Else:\n";
            printBlock(os, *node.elseBlock, level + 1, withLoc);
        }
    }, n);
}

void AstPrinter::printInteraction(std::ostream& os, const ast::Interaction& n, int level, bool withLoc) {
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        indent(os, level);

        if constexpr (std::is_same_v<T, ast::Comm>) {
            os << "Comm ";
            printProcExpr(os, node.from, withLoc);
            os << " -> ";
            printProcVar(os, node.to, withLoc);
            printLoc(os, node.loc, withLoc);
            os << "\n";
        } else if constexpr (std::is_same_v<T, ast::Select>) {
            os << "Select " << node.from << " -> " << node.to << " [" << node.label << "]";
            printLoc(os, node.loc, withLoc);
            os << "\n";
        } else if constexpr (std::is_same_v<T, ast::Assign>) {
            os << "Assign ";
            printProcVar(os, node.target, withLoc);
            os << " = ";
            printExpr(os, node.value, withLoc);
            printLoc(os, node.loc, withLoc);
            os << "\n";
        } else if constexpr (std::is_same_v<T, ast::Race>) {
            os << "Race ";
            printRaceId(os, node.id, withLoc);
            os << " : ";
            printProcExpr(os, node.left, withLoc);
            os << " , ";
            printProcExpr(os, node.right, withLoc);
            os << " -> ";
            printProcVar(os, node.target, withLoc);
            printLoc(os, node.loc, withLoc);
            os << "\n";
        } else if constexpr (std::is_same_v<T, ast::Discharge>) {
            os << "Discharge ";
            printRaceId(os, node.id, withLoc);
            os << " : " << node.source << " -> ";
            printProcVar(os, node.target, withLoc);
            printLoc(os, node.loc, withLoc);
            os << "\n";
        }
    }, n);
}

void AstPrinter::printExpr(std::ostream& os, const ast::Expr& n, bool withLoc) {
    (void)withLoc;
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ast::ExprVar>) {
            os << node.name;
        } else if constexpr (std::is_same_v<T, ast::Value>) {
            if (node.kind == ast::Value::Kind::Int) os << node.intValue;
            else os << (node.boolValue ? "true" : "false");
        }
    }, n);
}

void AstPrinter::printProcExpr(std::ostream& os, const ast::ProcExpr& n, bool withLoc) {
    (void)withLoc;
    os << n.process << ".";
    printExpr(os, n.expr, withLoc);
}

void AstPrinter::printProcVar(std::ostream& os, const ast::ProcVar& n, bool withLoc) {
    (void)withLoc;
    os << n.process << "." << n.var;
}

void AstPrinter::printRaceId(std::ostream& os, const ast::RaceId& n, bool withLoc) {
    (void)withLoc;
    os << n.process << "[" << n.key << "]";
}
