#pragma once
#include <string>
#include <vector>

#include "ast/SourceLocation.h"

namespace runtime {

struct TraceEvent {
    std::string kind;     // "asg", "com", "race", "if", ...
    std::string message;  // printable line
    ast::SourceRange loc; // where it happened (best-effort)

    // For CLI trace
    std::string toString() const {
        // Example: asg @file:line:col message
        std::string out = kind;
        if (!loc.file.empty()) {
            out += " @" + loc.file + ":" + std::to_string(loc.start.line) + ":" + std::to_string(loc.start.col);
        }
        out += " " + message;
        return out;
    }
};

using Trace = std::vector<TraceEvent>;

} // namespace runtime