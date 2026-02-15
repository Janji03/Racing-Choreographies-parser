#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

#include "antlr4-runtime.h"
#include "RacingChoreoLexer.h"
#include "RacingChoreoParser.h"

#include "ErrorListener.h"
#include "ast/Ast.h"
#include "AstBuilderVisitor.h"
#include "AstPrinter.h"
#include "Json.h"
#include "AstJson.h"
#include "Validation.h"

static constexpr const char* RC_PARSER_VERSION = "1.0.0";

// -------------------- IO helpers --------------------
static std::string readFileToString(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string readStdinToString() {
    std::ostringstream ss;
    ss << std::cin.rdbuf();
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

// -------------------- CLI help --------------------
static void printUsage(std::ostream& os) {
    os
        << "rc_parser - Racing Choreographies parser\n\n"
        << "Usage:\n"
        << "  rc_parser --help | -h\n"
        << "  rc_parser --version\n"
        << "  rc_parser parse  <file.rc> [--quiet] [--print-tree] [--json]\n"
        << "  rc_parser tokens <file.rc> [--quiet] [--json]\n"
        << "  rc_parser ast    <file.rc> [--quiet] [--print-tree] [--with-loc] [--json]\n"
        << "  rc_parser <cmd>  --stdin   [options]\n"
        << "  rc_parser <cmd>  --        (alias of --stdin)\n\n"
        << "Options:\n"
        << "  --quiet       No output (only exit code)\n"
        << "  --print-tree  Print ANTLR parse tree (CST)\n"
        << "  --with-loc    Include source locations in AST pretty print\n"
        << "  --json        Emit JSON\n\n"
        << "Notes:\n"
        << "  Exit codes: 0 OK, 1 syntax/lexical/validation error, 2 usage/io error\n";
}

static void printVersion(std::ostream& os) {
    os << "rc_parser " << RC_PARSER_VERSION << "\n";
}

// -------------------- Diagnostics --------------------
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

static void printPrettyValidationError(const ValidationError& err,
                                       const std::vector<std::string>& lines) {
    std::cerr << err.file << ":" << err.line << ":" << err.col
              << ": error: " << err.message << "\n";

    if (err.line == 0 || err.line > lines.size()) return;

    const std::string& srcLine = lines[err.line - 1];
    std::cerr << "  " << srcLine << "\n";

    std::cerr << "  ";
    for (size_t i = 0; i < err.col && i < srcLine.size(); ++i) {
        std::cerr << (srcLine[i] == '\t' ? '\t' : ' ');
    }
    std::cerr << "^\n";
}

static int printSyntaxErrorsAndFail(const ErrorListener& errorListener,
                                    const std::vector<std::string>& lines) {
    for (const auto& err : errorListener.errors()) {
        printPrettyError(err, lines);
    }
    return 1;
}

static int printValidationErrorsAndFail(const std::vector<ValidationError>& errs,
                                        const std::vector<std::string>& lines) {
    for (const auto& e : errs) {
        printPrettyValidationError(e, lines);
    }
    return 1;
}

static void printJsonErrors(json::Writer& w, const ErrorListener& el) {
    w.beginArray("errors");
    for (const auto& e : el.errors()) {
        w.elementObjectBegin();
        w.keyString("file", e.file);
        w.keyInt("line", static_cast<int>(e.line));
        w.keyInt("column", static_cast<int>(e.column));
        w.keyString("message", e.message);
        w.keyString("offendingText", e.offendingText);
        w.elementObjectEnd();
    }
    w.endArray();
}

static void printJsonValidationErrors(json::Writer& w,
                                      const std::vector<ValidationError>& errs) {
    w.beginArray("validationErrors");
    for (const auto& e : errs) {
        w.elementObjectBegin();
        w.keyString("file", e.file);
        w.keyInt("line", static_cast<int>(e.line));
        w.keyInt("column", static_cast<int>(e.col));
        w.keyString("message", e.message);
        w.elementObjectEnd();
    }
    w.endArray();
}

static void printJsonHeader(json::Writer& w,
                            const std::string& command,
                            const std::string& sourceName,
                            bool ok) {
    w.keyString("command", command);
    w.keyString("source", sourceName);
    w.keyBool("ok", ok);
}

// -------------------- Pipeline --------------------
struct Pipeline {
    std::string filePath;
    std::string input;
    std::vector<std::string> lines;

    antlr4::ANTLRInputStream inputStream;
    RacingChoreoLexer lexer;
    antlr4::CommonTokenStream tokens;
    RacingChoreoParser parser;

    ErrorListener errorListener;

    Pipeline(const std::string& file, const std::string& text)
        : filePath(file),
          input(text),
          lines(splitLines(text)),
          inputStream(input),
          lexer(&inputStream),
          tokens(&lexer),
          parser(&tokens),
          errorListener(filePath) {
        lexer.removeErrorListeners();
        parser.removeErrorListeners();
        lexer.addErrorListener(&errorListener);
        parser.addErrorListener(&errorListener);
    }
};

// -------------------- Run options --------------------
struct RunOptions {
    bool quiet = false;
    bool printTree = false;
    bool withLoc = false;
    bool json = false;
};

// -------------------- Commands --------------------
static int runParseFromText(const std::string& sourceName,
                            const std::string& text,
                            const RunOptions& opt) {
    Pipeline p(sourceName, text);
    auto* tree = p.parser.program();

    if (p.errorListener.hasErrors()) {
        if (opt.json) {
            json::Writer w(std::cout, 2);
            w.beginObject();
            printJsonHeader(w, "parse", sourceName, false);
            printJsonErrors(w, p.errorListener);

            // no validation if syntax fails
            w.beginArray("validationErrors");
            w.endArray();

            w.endObject();
            std::cout << "\n";
            return 1;
        }
        return printSyntaxErrorsAndFail(p.errorListener, p.lines);
    }

    // validation requires AST
    AstBuilderVisitor builder(sourceName);
    auto astProgram = builder.build(tree);

    Validator validator;
    auto vErrors = validator.validate(*astProgram);

    const bool ok = vErrors.empty();

    if (opt.json) {
        json::Writer w(std::cout, 2);
        w.beginObject();
        printJsonHeader(w, "parse", sourceName, ok);
        printJsonErrors(w, p.errorListener);
        printJsonValidationErrors(w, vErrors);

        if (opt.printTree) {
            w.keyString("cst", antlr4::tree::Trees::toStringTree(tree, &p.parser));
        }

        w.endObject();
        std::cout << "\n";
        return ok ? 0 : 1;
    }

    if (!ok) {
        return printValidationErrorsAndFail(vErrors, p.lines);
    }

    if (opt.printTree) {
        std::cout << antlr4::tree::Trees::toStringTree(tree, &p.parser) << "\n";
        return 0;
    }

    if (!opt.quiet) {
        std::cout << "Parse OK\n";
    }
    return 0;
}

static int runTokensFromText(const std::string& sourceName,
                             const std::string& text,
                             const RunOptions& opt) {
    Pipeline p(sourceName, text);
    p.tokens.fill();

    if (p.errorListener.hasErrors()) {
        if (opt.json) {
            json::Writer w(std::cout, 2);
            w.beginObject();
            printJsonHeader(w, "tokens", sourceName, false);
            printJsonErrors(w, p.errorListener);
            w.endObject();
            std::cout << "\n";
            return 1;
        }
        return printSyntaxErrorsAndFail(p.errorListener, p.lines);
    }

    if (opt.json) {
        json::Writer w(std::cout, 2);
        w.beginObject();
        printJsonHeader(w, "tokens", sourceName, true);
        printJsonErrors(w, p.errorListener);

        w.beginArray("tokens");
        for (antlr4::Token* t : p.tokens.getTokens()) {
            w.elementObjectBegin();
            w.keyInt("line", static_cast<int>(t->getLine()));
            w.keyInt("column", static_cast<int>(t->getCharPositionInLine()));

            const auto typeView = p.lexer.getVocabulary().getSymbolicName(t->getType());
            const std::string typeName(typeView.begin(), typeView.end());
            w.keyString("type", typeName.empty() ? "<UNKNOWN>" : typeName);

            w.keyString("text", t->getText());
            w.elementObjectEnd();
        }
        w.endArray();

        w.endObject();
        std::cout << "\n";
        return 0;
    }

    if (opt.quiet) return 0;

    for (antlr4::Token* t : p.tokens.getTokens()) {
        const auto typeView = p.lexer.getVocabulary().getSymbolicName(t->getType());
        const std::string typeName(typeView.begin(), typeView.end());
        const std::string tokenText = t->getText();

        std::cout << t->getLine() << ":" << t->getCharPositionInLine()
                  << "  " << (typeName.empty() ? "<UNKNOWN>" : typeName)
                  << "  \"" << tokenText << "\"\n";
    }

    return 0;
}

static int runAstFromText(const std::string& sourceName,
                          const std::string& text,
                          const RunOptions& opt) {
    Pipeline p(sourceName, text);
    auto* tree = p.parser.program();

    if (p.errorListener.hasErrors()) {
        if (opt.json) {
            json::Writer w(std::cout, 2);
            w.beginObject();
            printJsonHeader(w, "ast", sourceName, false);
            printJsonErrors(w, p.errorListener);

            w.beginArray("validationErrors");
            w.endArray();

            w.endObject();
            std::cout << "\n";
            return 1;
        }
        return printSyntaxErrorsAndFail(p.errorListener, p.lines);
    }

    AstBuilderVisitor builder(sourceName);
    auto astProgram = builder.build(tree);

    Validator validator;
    auto vErrors = validator.validate(*astProgram);
    const bool ok = vErrors.empty();

    if (opt.json) {
        json::Writer w(std::cout, 2);
        w.beginObject();
        printJsonHeader(w, "ast", sourceName, ok);
        printJsonErrors(w, p.errorListener);
        printJsonValidationErrors(w, vErrors);

        if (opt.printTree) {
            w.keyString("cst", antlr4::tree::Trees::toStringTree(tree, &p.parser));
        }

        w.keyRaw("ast", astjson::serialize(*astProgram));

        w.endObject();
        std::cout << "\n";
        return ok ? 0 : 1;
    }

    if (!ok) {
        return printValidationErrorsAndFail(vErrors, p.lines);
    }

    if (opt.printTree) {
        std::cout << antlr4::tree::Trees::toStringTree(tree, &p.parser) << "\n";
        return 0;
    }

    if (!opt.quiet) {
        AstPrinter::print(std::cout, *astProgram, opt.withLoc);
    }
    return 0;
}

// -------------------- Main --------------------
int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            const std::string arg = argv[1];
            if (arg == "--help" || arg == "-h") {
                printUsage(std::cout);
                return 0;
            }
            if (arg == "--version") {
                printVersion(std::cout);
                return 0;
            }
            printUsage(std::cerr);
            return 2;
        }

        if (argc < 3 || argc > 7) {
            printUsage(std::cerr);
            return 2;
        }

        const std::string command = argv[1];
        RunOptions opt;
        const std::string inputArg = argv[2];

        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--quiet") opt.quiet = true;
            else if (a == "--print-tree") opt.printTree = true;
            else if (a == "--with-loc") opt.withLoc = true;
            else if (a == "--json") opt.json = true;
            else {
                std::cerr << "Unknown option: " << a << "\n";
                printUsage(std::cerr);
                return 2;
            }
        }

        const bool useStdin = (inputArg == "--stdin" || inputArg == "--");
        const std::string sourceName = useStdin ? "<stdin>" : inputArg;
        std::string text = useStdin ? readStdinToString() : readFileToString(inputArg);

        if (command == "parse")  return runParseFromText(sourceName, text, opt);
        if (command == "tokens") return runTokensFromText(sourceName, text, opt);
        if (command == "ast")    return runAstFromText(sourceName, text, opt);

        printUsage(std::cerr);
        return 2;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2;
    }
}
