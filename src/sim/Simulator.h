#pragma once
#include "ast/Ast.h"
#include "sim/SimOptions.h"
#include "sim/SimulationResult.h"

namespace sim {

class Simulator final {
public:
    static SimulationResult run(const ast::Program& program, const SimOptions& opt);
};

} // namespace sim