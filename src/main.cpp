#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "antlr4-runtime.h"
#include "RacingChoreoLexer.h"
#include "RacingChoreoParser.h"

#include "ErrorListener.h"

static std::string readFileToString(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  rc_parser parse  <file.rc>\n"
        << "  rc_parser tokens <file.rc>\n";
}

static int runParse(const std::string& filePath) {
    const std::string input = readFileToString(filePath);

    antlr4::ANTLRInputStream inputStream(input);
    RacingChoreoLexer lexer(&inputStream);

    antlr4::CommonTokenStream tokens(&lexer);
    tokens.fill();

    RacingChoreoParser parser(&tokens);

    lexer.removeErrorListeners();
    parser.removeErrorListeners();

    ErrorListener errorListener;
    lexer.addErrorListener(&errorListener);
    parser.addErrorListener(&errorListener);

    parser.program();

    if (errorListener.hasErrors()) {
        for (const auto& err : errorListener.errors()) {
            std::cerr << "Syntax error at " << err.line << ":" << err.column
                      << " - " << err.message << "\n";
        }
        return 1;
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

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            printUsage();
            return 2;
        }

        const std::string command = argv[1];
        const std::string filePath = argv[2];

        if (command == "parse") {
            return runParse(filePath);
        }
        if (command == "tokens") {
            return runTokens(filePath);
        }

        printUsage();
        return 2;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2;
    }
}
