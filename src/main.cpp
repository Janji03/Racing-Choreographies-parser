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

// Simulator
#include "sim/Simulator.h"
#include "sim/SimOptions.h"
#include "sim/SimulationResult.h"
#include "runtime/Value.h"
#include "runtime/Store.h"
#include "runtime/Trace.h"
#include "runtime/RaceMemory.h"

static constexpr const char* RC_PARSER_VERSION = "4.0.0";

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
        << "  rc_parser parse     <file.rc> [--quiet] [--print-tree] [--json]\n"
        << "  rc_parser tokens    <file.rc> [--quiet] [--json]\n"
        << "  rc_parser ast       <file.rc> [--quiet] [--print-tree] [--with-loc] [--json]\n"
        << "  rc_parser simulate  <file.rc> [--quiet] [--json] [--trace|--no-trace] [--final-store] [--final-races]\n"
        << "  rc_parser <cmd>     --stdin   [options]\n"
        << "  rc_parser <cmd>     --        (alias of --stdin)\n\n"
        << "Options (common):\n"
        << "  --quiet       No output (only exit code)\n"
        << "  --print-tree  Print ANTLR parse tree (CST)\n"
        << "  --with-loc    Include source locations in AST pretty print\n"
        << "  --json        Emit JSON\n\n"
        << "Notes:\n"
        << "  Exit codes: 0 OK, 1 syntax/lexical/validation/runtime error, 2 usage/io error\n";
}

static void printSimUsage(std::ostream& os) {
    os
        << "rc_parser simulate - Racing Choreographies simulator\n\n"
        << "Usage:\n"
        << "  rc_parser simulate --help\n"
        << "  rc_parser simulate <file.rc> [--stdin|--] [options]\n\n"
        << "Options:\n"
        << "  --quiet            No output (only exit code)\n"
        << "  --json             Emit JSON result\n"
        << "  --trace            Print step-by-step trace (default)\n"
        << "  --no-trace         Disable trace output\n"
        << "  --final-store      Print final store (Sigma)\n"
        << "  --final-races      Print final race memory M\n"
        << "  --seed N           Seed for random race policy\n"
        << "  --race MODE        MODE = left|right|random\n"
        << "  --max-steps N      Max executed steps (default 100000)\n"
        << "  --max-call-depth N Max call depth (default 1000)\n"
        << "  --init P.X=V       Initialize store entry (repeatable), V=int|true|false\n"
        << "                    Example: --init c.req=5 --init w1.req=5 --init w2.req=5\n";
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

// JSON helpers (existing)
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

// -------------------- Run options (parser/ast/tokens) --------------------
struct RunOptions {
    bool quiet = false;
    bool printTree = false;
    bool withLoc = false;
    bool json = false;
};

// -------------------- Commands: parse/tokens/ast --------------------
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

// -------------------- Simulator command --------------------
struct SimCliOptions {
    sim::SimOptions simOpt;
    bool help = false;
};

static bool parseU64(const std::string& s, uint64_t& out) {
    try {
        size_t idx = 0;
        unsigned long long v = std::stoull(s, &idx, 10);
        if (idx != s.size()) return false;
        out = static_cast<uint64_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

static bool parseI64(const std::string& s, int64_t& out) {
    try {
        size_t idx = 0;
        long long v = std::stoll(s, &idx, 10);
        if (idx != s.size()) return false;
        out = static_cast<int64_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

// Parse "P.X=V" where V is int|true|false
static bool parseInitBinding(const std::string& s, sim::InitBinding& out) {
    // find '='
    const auto eq = s.find('=');
    if (eq == std::string::npos) return false;

    const std::string lhs = s.substr(0, eq);
    const std::string rhs = s.substr(eq + 1);

    // lhs must be "P.X"
    const auto dot = lhs.find('.');
    if (dot == std::string::npos) return false;
    const std::string proc = lhs.substr(0, dot);
    const std::string var  = lhs.substr(dot + 1);

    if (proc.empty() || var.empty()) return false;
    if (rhs.empty()) return false;

    // rhs bool?
    if (rhs == "true") {
        out.process = proc;
        out.var = var;
        out.value = runtime::Value::makeBool(true);
        return true;
    }
    if (rhs == "false") {
        out.process = proc;
        out.var = var;
        out.value = runtime::Value::makeBool(false);
        return true;
    }

    // rhs int
    int64_t iv = 0;
    if (!parseI64(rhs, iv)) return false;

    out.process = proc;
    out.var = var;
    out.value = runtime::Value::makeInt(static_cast<int>(iv));
    return true;
}

static SimCliOptions parseSimOptions(int argc, char** argv, int startIndex, std::ostream& err, bool& ok) {
    SimCliOptions opt;
    ok = true;

    for (int i = startIndex; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--help" || a == "-h") {
            opt.help = true;
        } else if (a == "--quiet") {
            opt.simOpt.quiet = true;
        } else if (a == "--json") {
            opt.simOpt.json = true;
        } else if (a == "--trace") {
            opt.simOpt.trace = true;
        } else if (a == "--no-trace") {
            opt.simOpt.trace = false;
        } else if (a == "--final-store") {
            opt.simOpt.finalStore = true;
        } else if (a == "--final-races") {
            opt.simOpt.finalRaces = true;
        } else if (a == "--seed") {
            if (i + 1 >= argc) { err << "Missing value for --seed\n"; ok = false; return opt; }
            uint64_t v = 0;
            if (!parseU64(argv[++i], v)) { err << "Invalid --seed value\n"; ok = false; return opt; }
            opt.simOpt.seed = v;
        } else if (a == "--race") {
            if (i + 1 >= argc) { err << "Missing value for --race\n"; ok = false; return opt; }
            const std::string mode = argv[++i];
            if (mode == "left") opt.simOpt.racePolicy = sim::RacePolicy::Left;
            else if (mode == "right") opt.simOpt.racePolicy = sim::RacePolicy::Right;
            else if (mode == "random") opt.simOpt.racePolicy = sim::RacePolicy::Random;
            else { err << "Invalid --race mode: " << mode << "\n"; ok = false; return opt; }
        } else if (a == "--max-steps") {
            if (i + 1 >= argc) { err << "Missing value for --max-steps\n"; ok = false; return opt; }
            uint64_t v = 0;
            if (!parseU64(argv[++i], v)) { err << "Invalid --max-steps value\n"; ok = false; return opt; }
            opt.simOpt.maxSteps = v;
        } else if (a == "--max-call-depth") {
            if (i + 1 >= argc) { err << "Missing value for --max-call-depth\n"; ok = false; return opt; }
            uint64_t v = 0;
            if (!parseU64(argv[++i], v)) { err << "Invalid --max-call-depth value\n"; ok = false; return opt; }
            opt.simOpt.maxCallDepth = v;
        } else if (a == "--init") {
            if (i + 1 >= argc) { err << "Missing value for --init\n"; ok = false; return opt; }
            sim::InitBinding b;
            if (!parseInitBinding(argv[++i], b)) {
                err << "Invalid --init format: expected P.X=V with V=int|true|false\n";
                ok = false;
                return opt;
            }
            opt.simOpt.init.push_back(std::move(b));
        } else {
            err << "Unknown option for simulate: " << a << "\n";
            ok = false;
            return opt;
        }
    }

    return opt;
}

static void printFinalStore(std::ostream& os, const runtime::Store& store) {
    os << "Final Store Sigma:\n";
    const auto& m = store.raw();
    if (m.empty()) {
        os << "  <empty>\n";
        return;
    }

    std::vector<std::string> keys;
    keys.reserve(m.size());
    for (const auto& kv : m) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());

    for (const auto& k : keys) {
        os << "  " << k << " = " << m.at(k).toString() << "\n";
    }
}

static void printFinalRaces(std::ostream& os, const runtime::RaceMemory& M) {
    os << "Final Races M:\n";
    const auto& raw = M.raw();
    if (raw.empty()) {
        os << "  <empty>\n";
        return;
    }

    for (const auto& kv : raw) {
        const auto& k = kv.first;
        const auto& e = kv.second;

        os << "  " << k.process << "[" << k.key << "]: "
           << "left=" << e.leftProc << ", right=" << e.rightProc
           << ", winner=" << e.winnerProc << ", loser=" << e.loserProc
           << ", vWin=" << e.vWinner.toString() << ", vLose=" << e.vLoser.toString()
           << ", discharged=" << (e.discharged ? "true" : "false")
           << "\n";
    }
}

static void printJsonTrace(json::Writer& w, const runtime::Trace& trace) {
    w.beginArray("trace");
    for (const auto& ev : trace) {
        w.elementObjectBegin();
        w.keyString("kind", ev.kind);
        w.keyString("message", ev.message);
        w.keyString("file", ev.loc.file);
        w.keyInt("line", static_cast<int>(ev.loc.start.line));
        w.keyInt("column", static_cast<int>(ev.loc.start.col));
        w.elementObjectEnd();
    }
    w.endArray();
}

static void printJsonFinalStore(json::Writer& w, const runtime::Store& store) {
    const auto& m = store.raw();

    std::vector<std::string> keys;
    keys.reserve(m.size());
    for (const auto& kv : m) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());

    w.beginArray("finalStore");
    for (const auto& k : keys) {
        const auto& v = m.at(k);

        w.elementObjectBegin();
        w.keyString("var", k);

        if (v.kind == runtime::Value::Kind::Int) {
            w.keyString("type", "int");
            w.keyInt("value", v.intValue);
        } else {
            w.keyString("type", "bool");
            w.keyBool("value", v.boolValue);
        }

        w.elementObjectEnd();
    }
    w.endArray();
}

static void printJsonFinalRaces(json::Writer& w,
                                const runtime::RaceMemory& M,
                                bool enabled) {
    w.beginArray("finalRaces");
    if (enabled) {
        for (const auto& kv : M.raw()) {
            const auto& k = kv.first;
            const auto& e = kv.second;

            w.elementObjectBegin();
            w.keyString("race", k.process + "[" + k.key + "]");
            w.keyString("process", k.process);
            w.keyString("key", k.key);

            w.keyString("left", e.leftProc);
            w.keyString("right", e.rightProc);
            w.keyString("winner", e.winnerProc);
            w.keyString("loser", e.loserProc);

            if (e.vWinner.kind == runtime::Value::Kind::Int) {
                w.keyString("vWinnerType", "int");
                w.keyInt("vWinner", e.vWinner.intValue);
            } else {
                w.keyString("vWinnerType", "bool");
                w.keyBool("vWinner", e.vWinner.boolValue);
            }

            if (e.vLoser.kind == runtime::Value::Kind::Int) {
                w.keyString("vLoserType", "int");
                w.keyInt("vLoser", e.vLoser.intValue);
            } else {
                w.keyString("vLoserType", "bool");
                w.keyBool("vLoser", e.vLoser.boolValue);
            }

            w.keyBool("discharged", e.discharged);
            w.elementObjectEnd();
        }
    }
    w.endArray();
}

static int runSimulateFromText(const std::string& sourceName,
                               const std::string& text,
                               const SimCliOptions& cliOpt) {
    Pipeline p(sourceName, text);
    auto* tree = p.parser.program();

    if (p.errorListener.hasErrors()) {
        if (cliOpt.simOpt.json) {
            json::Writer w(std::cout, 2);
            w.beginObject();
            printJsonHeader(w, "simulate", sourceName, false);
            printJsonErrors(w, p.errorListener);

            w.beginArray("validationErrors"); w.endArray();
            w.beginArray("runtimeErrors");    w.endArray();
            w.beginArray("trace");            w.endArray();
            w.beginArray("finalStore");       w.endArray();
            w.beginArray("finalRaces");       w.endArray();

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
    if (!vErrors.empty()) {
        if (cliOpt.simOpt.json) {
            json::Writer w(std::cout, 2);
            w.beginObject();
            printJsonHeader(w, "simulate", sourceName, false);
            printJsonErrors(w, p.errorListener);
            printJsonValidationErrors(w, vErrors);

            w.beginArray("runtimeErrors"); w.endArray();
            w.beginArray("trace");         w.endArray();
            w.beginArray("finalStore");    w.endArray();
            w.beginArray("finalRaces");    w.endArray();

            w.endObject();
            std::cout << "\n";
            return 1;
        }
        return printValidationErrorsAndFail(vErrors, p.lines);
    }

    sim::SimulationResult res = sim::Simulator::run(*astProgram, cliOpt.simOpt);

    if (cliOpt.simOpt.json) {
        json::Writer w(std::cout, 2);
        w.beginObject();
        printJsonHeader(w, "simulate", sourceName, res.ok);
        printJsonErrors(w, p.errorListener);

        w.beginArray("validationErrors"); w.endArray();

        w.beginArray("runtimeErrors");
        for (const auto& e : res.runtimeErrors) {
            w.elementObjectBegin();
            w.keyString("file", e.file);
            w.keyInt("line", static_cast<int>(e.line));
            w.keyInt("column", static_cast<int>(e.col));
            w.keyString("message", e.message);
            w.elementObjectEnd();
        }
        w.endArray();

        printJsonTrace(w, res.trace);
        printJsonFinalStore(w, res.store);
        printJsonFinalRaces(w, res.races, cliOpt.simOpt.finalRaces);

        w.endObject();
        std::cout << "\n";
        return res.ok ? 0 : 1;
    }

    if (!cliOpt.simOpt.quiet) {
        if (cliOpt.simOpt.trace) {
            for (const auto& ev : res.trace) {
                std::cout << ev.toString() << "\n";
            }
        }

        if (cliOpt.simOpt.finalStore) {
            printFinalStore(std::cout, res.store);
        }

        if (cliOpt.simOpt.finalRaces) {
            printFinalRaces(std::cout, res.races);
        }

        for (const auto& e : res.runtimeErrors) {
            std::cerr << e.file << ":" << e.line << ":" << e.col
                      << ": runtime error: " << e.message << "\n";
        }
    }

    return res.ok ? 0 : 1;
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

        if (argc == 3) {
            const std::string command = argv[1];
            const std::string arg2 = argv[2];
            if (command == "simulate" && (arg2 == "--help" || arg2 == "-h")) {
                printSimUsage(std::cout);
                return 0;
            }
        }

        if (argc < 3 || argc > 64) {
            printUsage(std::cerr);
            return 2;
        }

        const std::string command = argv[1];
        RunOptions opt;
        const std::string inputArg = argv[2];

        if (command == "simulate") {
            const bool useStdin = (inputArg == "--stdin" || inputArg == "--");
            const std::string sourceName = useStdin ? "<stdin>" : inputArg;
            std::string text = useStdin ? readStdinToString() : readFileToString(inputArg);

            bool ok = true;
            SimCliOptions simCli = parseSimOptions(argc, argv, 3, std::cerr, ok);
            if (!ok) {
                printSimUsage(std::cerr);
                return 2;
            }
            if (simCli.help) {
                printSimUsage(std::cout);
                return 0;
            }
            return runSimulateFromText(sourceName, text, simCli);
        }

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