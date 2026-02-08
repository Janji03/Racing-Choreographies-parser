#include "AstPrinter.h"

#include <iostream>
#include <variant>

using namespace std;

void AstPrinter::print(std::ostream& os, const ast::Program& program) {
    printProgram(os, program, 0);
}

void AstPrinter::indent(std::ostream& os, int level) {
    for (int i = 0; i < level; ++i) os << "  ";
}

void AstPrinter::printProgram(std::ostream& os, const ast::Program& n, int level) {
    indent(os, level);
    os << "Program\n";

    indent(os, level + 1);
    os << "Procedures (" << n.procedures.size() << ")\n";
    for (const auto& p : n.procedures) {
        printProcDef(os, *p, level + 2);
    }

    indent(os, level + 1);
    os << "Main\n";
    printMain(os, *n.main, level + 2);
}

void AstPrinter::printProcDef(std::ostream& os, const ast::ProcDef& n, int level) {
    indent(os, level);
    os << "ProcDef " << n.name << "(";
    for (size_t i = 0; i < n.params.size(); ++i) {
        if (i) os << ",";
        os << n.params[i];
    }
    os << ")\n";
    printBlock(os, *n.body, level + 1);
}

void AstPrinter::printMain(std::ostream& os, const ast::Main& n, int level) {
    printBlock(os, *n.body, level);
}

void AstPrinter::printBlock(std::ostream& os, const ast::Block& n, int level) {
    indent(os, level);
    os << "Block (" << n.statements.size() << " stmt)\n";
    for (const auto& s : n.statements) {
        printStmt(os, *s, level + 1);
    }
}

void AstPrinter::printStmt(std::ostream& os, const ast::Stmt& n, int level) {
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ast::InteractionStmt>) {
            indent(os, level);
            os << "InteractionStmt\n";
            printInteraction(os, node.interaction, level + 1);
        } else if constexpr (std::is_same_v<T, ast::CallStmt>) {
            indent(os, level);
            os << "Call " << node.proc << "(";
            for (size_t i = 0; i < node.args.size(); ++i) {
                if (i) os << ",";
                os << node.args[i];
            }
            os << ")\n";
        } else if constexpr (std::is_same_v<T, ast::IfLocalStmt>) {
            indent(os, level);
            os << "IfLocal (";
            printProcExpr(os, node.condition);
            os << ")\n";
            indent(os, level);
            os << "Then:\n";
            printBlock(os, *node.thenBlock, level + 1);
            indent(os, level);
            os << "Else:\n";
            printBlock(os, *node.elseBlock, level + 1);
        } else if constexpr (std::is_same_v<T, ast::IfRaceStmt>) {
            indent(os, level);
            os << "IfRace (";
            printRaceId(os, node.condition);
            os << ")\n";
            indent(os, level);
            os << "Then:\n";
            printBlock(os, *node.thenBlock, level + 1);
            indent(os, level);
            os << "Else:\n";
            printBlock(os, *node.elseBlock, level + 1);
        }
    }, n);
}

void AstPrinter::printInteraction(std::ostream& os, const ast::Interaction& n, int level) {
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        indent(os, level);

        if constexpr (std::is_same_v<T, ast::Comm>) {
            os << "Comm ";
            printProcExpr(os, node.from);
            os << " -> ";
            printProcVar(os, node.to);
            os << "\n";
        } else if constexpr (std::is_same_v<T, ast::Select>) {
            os << "Select " << node.from << " -> " << node.to << " [" << node.label << "]\n";
        } else if constexpr (std::is_same_v<T, ast::Assign>) {
            os << "Assign ";
            printProcVar(os, node.target);
            os << " = ";
            printExpr(os, node.value);
            os << "\n";
        } else if constexpr (std::is_same_v<T, ast::Race>) {
            os << "Race ";
            printRaceId(os, node.id);
            os << " : ";
            printProcExpr(os, node.left);
            os << " , ";
            printProcExpr(os, node.right);
            os << " -> ";
            printProcVar(os, node.target);
            os << "\n";
        } else if constexpr (std::is_same_v<T, ast::Discharge>) {
            os << "Discharge ";
            printRaceId(os, node.id);
            os << " : " << node.source << " -> ";
            printProcVar(os, node.target);
            os << "\n";
        }
    }, n);
}

void AstPrinter::printExpr(std::ostream& os, const ast::Expr& n) {
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

void AstPrinter::printProcExpr(std::ostream& os, const ast::ProcExpr& n) {
    os << n.process << ".";
    printExpr(os, n.expr);
}

void AstPrinter::printProcVar(std::ostream& os, const ast::ProcVar& n) {
    os << n.process << "." << n.var;
}

void AstPrinter::printRaceId(std::ostream& os, const ast::RaceId& n) {
    os << n.process << "[" << n.key << "]";
}
