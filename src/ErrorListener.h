#pragma once
#include <antlr4-runtime.h>
#include <string>
#include <vector>

class ErrorListener final : public antlr4::BaseErrorListener {
public:
    struct SyntaxError {
        size_t line;
        size_t column;
        std::string message;
    };

    void syntaxError(antlr4::Recognizer *recognizer,
                     antlr4::Token *offendingSymbol,
                     size_t line,
                     size_t charPositionInLine,
                     const std::string &msg,
                     std::exception_ptr e) override;

    bool hasErrors() const { return !errors_.empty(); }
    const std::vector<SyntaxError>& errors() const { return errors_; }

private:
    std::vector<SyntaxError> errors_;
};
