#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "antlr4-runtime.h"
#include "RacingChoreoLexer.h"
#include "RacingChoreoParser.h"

#include "ErrorListener.h"
#include "ast/Ast.h"
#include "AstBuilderVisitor.h"
#include "AstPrinter.h"

static std::string readFileToString(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            if (!current.empty() && current.back() == '\r') current.pop_back();
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        if (!current.empty() && current.back() == '\r') current.pop_back();
        lines.push_back(current);
    }
    return lines;
}

static void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  rc_parser parse  <file.rc>\n"
        << "  rc_parser tokens <file.rc>\n"
        << "  rc_parser ast    <file.rc>\n";
}

static void attachErrorListeners(RacingChoreoLexer& lexer,
                                 RacingChoreoParser& parser,
                                 ErrorListener& errorListener) {
    lexer.removeErrorListeners();
    parser.removeErrorListeners();
    lexer.addErrorListener(&errorListener);
    parser.addErrorListener(&errorListener);
}

static void printPrettyError(const ErrorListener::SyntaxError& err,
                             const std::vector<std::string>& lines) {
    std::cerr << err.file << ":" << err.line << ":" << err.column
              << ": error: " << err.message;

    if (!err.offendingText.empty()) {
        std::cerr << " (at '" << err.offendingText << "')";
    }
    std::cerr << "\n";

    if (err.line == 0 || err.line > lines.size()) return;

    const std::string& srcLine = lines[err.line - 1];
    std::cerr << "  " << srcLine << "\n";

    std::cerr << "  ";
    for (size_t i = 0; i < err.column && i < srcLine.size(); ++i) {
        std::cerr << (srcLine[i] == '\t' ? '\t' : ' ');
    }
    std::cerr << "^\n";
}

static int printErrorsAndFail(const ErrorListener& errorListener,
                              const std::vector<std::string>& lines) {
    for (const auto& err : errorListener.errors()) {
        printPrettyError(err, lines);
    }
    return 1; // syntax error(s)
}

static int runParse(const std::string& filePath) {
    const std::string input = readFileToString(filePath);
    const auto lines = splitLines(input);

    antlr4::ANTLRInputStream inputStream(input);
    RacingChoreoLexer lexer(&inputStream);

    antlr4::CommonTokenStream tokens(&lexer);
    tokens.fill();

    RacingChoreoParser parser(&tokens);

    ErrorListener errorListener(filePath);
    attachErrorListeners(lexer, parser, errorListener);

    parser.program();

    if (errorListener.hasErrors()) {
        return printErrorsAndFail(errorListener, lines);
    }

    std::cout << "Parse OK\n";
    return 0;
}

static int runTokens(const std::string& filePath) {
    const std::string input = readFileToString(filePath);

    antlr4::ANTLRInputStream inputStream(input);
    RacingChoreoLexer lexer(&inputStream);

    antlr4::CommonTokenStream tokens(&lexer);
    tokens.fill();

    for (antlr4::Token* t : tokens.getTokens()) {
        const auto typeView = lexer.getVocabulary().getSymbolicName(t->getType());
        const std::string typeName(typeView.begin(), typeView.end());
        const std::string text = t->getText();

        std::cout << t->getLine() << ":" << t->getCharPositionInLine()
                  << "  " << (typeName.empty() ? "<UNKNOWN>" : typeName)
                  << "  \"" << text << "\"\n";
    }

    return 0;
}

static int runAst(const std::string& filePath) {
    const std::string input = readFileToString(filePath);
    const auto lines = splitLines(input);

    antlr4::ANTLRInputStream inputStream(input);
    RacingChoreoLexer lexer(&inputStream);

    antlr4::CommonTokenStream tokens(&lexer);
    tokens.fill();

    RacingChoreoParser parser(&tokens);

    ErrorListener errorListener(filePath);
    attachErrorListeners(lexer, parser, errorListener);

    auto* tree = parser.program();

    if (errorListener.hasErrors()) {
        return printErrorsAndFail(errorListener, lines);
    }

    AstBuilderVisitor builder;
    auto astProgram = builder.build(tree);

    AstPrinter::print(std::cout, *astProgram);
    return 0;
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            printUsage();
            return 2; // usage
        }

        const std::string command = argv[1];
        const std::string filePath = argv[2];

        if (command == "parse") {
            return runParse(filePath);
        }
        if (command == "tokens") {
            return runTokens(filePath);
        }
        if (command == "ast") {
            return runAst(filePath);
        }

        printUsage();
        return 2;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2; // io/internal
    }
}
