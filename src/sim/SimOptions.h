#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "runtime/Value.h"

namespace sim {

enum class RacePolicy { Random, Left, Right };

struct InitBinding {
    std::string process;
    std::string var;
    runtime::Value value;
};

struct SimOptions {
    bool quiet = false;
    bool json = false;

    bool trace = true;
    bool finalStore = false;
    bool finalRaces = false;

    uint64_t seed = 0;
    RacePolicy racePolicy = RacePolicy::Random;

    uint64_t maxSteps = 100000;
    uint64_t maxCallDepth = 1000;

    // CLI: --init p.x=5 / --init p.flag=true
    // IMPORTANT: main.cpp usa questo nome (init), non "inits".
    std::vector<InitBinding> init;
};

} // namespace sim