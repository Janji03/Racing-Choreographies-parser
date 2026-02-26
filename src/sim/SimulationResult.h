#pragma once
#include <string>
#include <vector>

#include "runtime/Store.h"
#include "runtime/RaceMemory.h"
#include "runtime/Trace.h"
#include "ast/SourceLocation.h"

namespace sim {

struct RuntimeErrorInfo {
    std::string file;
    uint32_t line = 0;
    uint32_t col  = 0;
    std::string message;
};

struct SimulationResult {
    bool ok = false;

    runtime::Trace trace;
    runtime::Store store;
    runtime::RaceMemory races;

    std::vector<RuntimeErrorInfo> runtimeErrors;
};

} // namespace sim