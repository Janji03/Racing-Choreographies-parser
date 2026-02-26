#pragma once
#include <stdexcept>
#include <string>

#include "ast/SourceLocation.h"

namespace runtime {

// RuntimeError: includes source location info for nice reporting
class RuntimeError final : public std::runtime_error {
public:
    RuntimeError(const ast::SourceRange& loc, const std::string& msg)
        : std::runtime_error(msg), loc_(loc) {}

    const ast::SourceRange& loc() const { return loc_; }

private:
    ast::SourceRange loc_;
};

} // namespace runtime