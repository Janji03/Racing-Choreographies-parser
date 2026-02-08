#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ast {

// ===== Identifiers =====
using Process  = std::string;
using Var      = std::string;
using Label    = std::string;
using ProcName = std::string;

// ===== Values & Expressions =====
struct Value {
    enum class Kind { Int, Bool };
    Kind kind;
    int intValue = 0;
    bool boolValue = false;
};

struct ExprVar {
    Var name;
};

using Expr = std::variant<Value, ExprVar>;

// ===== ProcExpr / ProcVar =====
struct ProcExpr {
    Process process;
    Expr expr;
};

struct ProcVar {
    Process process;
    Var var;
};

// ===== RaceId =====
struct RaceId {
    Process process;
    std::string key;
};

// ===== Interactions =====
struct Comm {
    ProcExpr from;
    ProcVar to;
};

struct Select {
    Process from;
    Process to;
    Label label;
};

struct Assign {
    ProcVar target;
    Expr value;
};

struct Race {
    RaceId id;
    ProcExpr left;
    ProcExpr right;
    ProcVar target;
};

struct Discharge {
    RaceId id;
    Process source;
    ProcVar target;
};

using Interaction = std::variant<Comm, Select, Assign, Race, Discharge>;

// ===== Statements =====
struct InteractionStmt {
    Interaction interaction;
};

struct CallStmt {
    ProcName proc;
    std::vector<Process> args;
};

struct IfLocalStmt {
    ProcExpr condition;
    std::unique_ptr<struct Block> thenBlock;
    std::unique_ptr<struct Block> elseBlock;
};

struct IfRaceStmt {
    RaceId condition;
    std::unique_ptr<struct Block> thenBlock;
    std::unique_ptr<struct Block> elseBlock;
};

using Stmt = std::variant<
    InteractionStmt,
    CallStmt,
    IfLocalStmt,
    IfRaceStmt
>;

// ===== Block =====
struct Block {
    std::vector<std::unique_ptr<Stmt>> statements;
};

// ===== Procedures & Program =====
struct ProcDef {
    ProcName name;
    std::vector<Process> params;
    std::unique_ptr<Block> body;
};

struct Main {
    std::unique_ptr<Block> body;
};

struct Program {
    std::vector<std::unique_ptr<ProcDef>> procedures;
    std::unique_ptr<Main> main;
};

} // namespace ast
