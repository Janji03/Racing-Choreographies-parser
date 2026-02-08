#include "ErrorListener.h"

void ErrorListener::syntaxError(antlr4::Recognizer*,
                                antlr4::Token* offendingSymbol,
                                size_t line,
                                size_t charPositionInLine,
                                const std::string& msg,
                                std::exception_ptr) {
    SyntaxError err;
    err.file = file_;
    err.line = line;
    err.column = charPositionInLine;
    err.message = msg;

    if (offendingSymbol) {
        err.offendingText = offendingSymbol->getText();
    }

    errors_.push_back(std::move(err));
}
