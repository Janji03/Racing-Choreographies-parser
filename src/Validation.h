#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "ast/Ast.h"

struct ValidationError {
    std::string file;
    uint32_t line = 0; // 1-based
    uint32_t col  = 0; // 0-based (ANTLR-style)
    std::string message;
};

class Validator final {
public:
    std::vector<ValidationError> validate(const ast::Program& program);

private:
    void validateProcTable(const ast::Program& program);
    void validateProgramBody(const ast::Program& program);

    void validateBlock(const ast::Block& b);
    void validateStmt(const ast::Stmt& st);

    void addError(const ast::SourceRange& loc, const std::string& msg);

private:
    struct ProcInfo {
        size_t arity = 0;
        ast::SourceRange loc;
    };

    std::vector<ValidationError> errors_;
    std::unordered_map<std::string, ProcInfo> procs_;
};
