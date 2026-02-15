#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "SourceLocation.h"

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

    SourceRange loc;
};

struct ExprVar {
    Var name;

    SourceRange loc;
};

using Expr = std::variant<Value, ExprVar>;

// ===== ProcExpr / ProcVar =====
struct ProcExpr {
    Process process;
    Expr expr;

    SourceRange loc;
};

struct ProcVar {
    Process process;
    Var var;

    SourceRange loc;
};

// ===== RaceId =====
struct RaceId {
    Process process;
    std::string key;

    SourceRange loc;
};

// ===== Interactions =====
struct Comm {
    ProcExpr from;
    ProcVar to;

    SourceRange loc;
};

struct Select {
    Process from;
    Process to;
    Label label;

    SourceRange loc;
};

struct Assign {
    ProcVar target;
    Expr value;

    SourceRange loc;
};

struct Race {
    RaceId id;
    ProcExpr left;
    ProcExpr right;
    ProcVar target;

    SourceRange loc;
};

struct Discharge {
    RaceId id;
    Process source;
    ProcVar target;

    SourceRange loc;
};

using Interaction = std::variant<Comm, Select, Assign, Race, Discharge>;

// ===== Statements =====
struct InteractionStmt {
    Interaction interaction;

    SourceRange loc;
};

struct CallStmt {
    ProcName proc;
    std::vector<Process> args;

    SourceRange loc;
};

struct IfLocalStmt {
    ProcExpr condition;
    std::unique_ptr<struct Block> thenBlock;
    std::unique_ptr<struct Block> elseBlock;

    SourceRange loc;
};

struct IfRaceStmt {
    RaceId condition;
    std::unique_ptr<struct Block> thenBlock;
    std::unique_ptr<struct Block> elseBlock;

    SourceRange loc;
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

    SourceRange loc;
};

// ===== Procedures & Program =====
struct ProcDef {
    ProcName name;
    std::vector<Process> params;
    std::unique_ptr<Block> body;

    SourceRange loc;
};

struct Main {
    std::unique_ptr<Block> body;

    SourceRange loc;
};

struct Program {
    std::vector<std::unique_ptr<ProcDef>> procedures;
    std::unique_ptr<Main> main;

    SourceRange loc;
};

} // namespace ast
