#include "Validation.h"

#include <sstream>
#include <variant>

using namespace std;

void Validator::addError(const ast::SourceRange& loc, const std::string& msg) {
    ValidationError e;
    e.file = loc.file;
    e.line = loc.start.line;
    e.col  = loc.start.col;
    e.message = msg;
    errors_.push_back(std::move(e));
}

void Validator::validateProcTable(const ast::Program& program) {
    procs_.clear();

    for (const auto& p : program.procedures) {
        auto it = procs_.find(p->name);
        if (it != procs_.end()) {
            std::ostringstream ss;
            ss << "duplicate procedure '" << p->name << "'";
            addError(p->loc, ss.str());
            continue;
        }

        ProcInfo info;
        info.arity = p->params.size();
        info.loc = p->loc;
        procs_.emplace(p->name, std::move(info));
    }
}

void Validator::validateStmt(const ast::Stmt& st) {
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ast::InteractionStmt>) {
            // no checks for now
        } else if constexpr (std::is_same_v<T, ast::CallStmt>) {
            auto it = procs_.find(node.proc);
            if (it == procs_.end()) {
                std::ostringstream ss;
                ss << "call to undefined procedure '" << node.proc << "'";
                addError(node.loc, ss.str());
                return;
            }

            const size_t expected = it->second.arity;
            const size_t got = node.args.size();
            if (expected != got) {
                std::ostringstream ss;
                ss << "wrong number of arguments in call to '" << node.proc
                   << "': expected " << expected << ", got " << got;
                addError(node.loc, ss.str());
            }
        } else if constexpr (std::is_same_v<T, ast::IfLocalStmt>) {
            validateBlock(*node.thenBlock);
            validateBlock(*node.elseBlock);
        } else if constexpr (std::is_same_v<T, ast::IfRaceStmt>) {
            validateBlock(*node.thenBlock);
            validateBlock(*node.elseBlock);
        }
    }, st);
}

void Validator::validateBlock(const ast::Block& b) {
    for (const auto& st : b.statements) {
        validateStmt(*st);
    }
}

void Validator::validateProgramBody(const ast::Program& program) {
    for (const auto& p : program.procedures) {
        validateBlock(*p->body);
    }
    validateBlock(*program.main->body);
}

std::vector<ValidationError> Validator::validate(const ast::Program& program) {
    errors_.clear();
    validateProcTable(program);
    validateProgramBody(program);
    return errors_;
}
