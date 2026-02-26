#pragma once
#include <string>
#include <unordered_map>
#include <optional>

#include "runtime/Value.h"

namespace runtime {

struct RaceKey {
    std::string process; // s
    std::string key;     // k

    bool operator==(const RaceKey& other) const {
        return process == other.process && key == other.key;
    }
};

struct RaceKeyHash {
    size_t operator()(const RaceKey& rk) const noexcept {
        std::hash<std::string> h;
        size_t a = h(rk.process);
        size_t b = h(rk.key);
        // simple hash combine
        return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
    }
};

enum class RaceWinnerSide { Left, Right };

struct RaceEntry {
    std::string leftProc;
    std::string rightProc;

    RaceWinnerSide winnerSide = RaceWinnerSide::Left;
    std::string winnerProc;
    std::string loserProc;

    Value vWinner;
    Value vLoser;

    bool discharged = false;
};

class RaceMemory {
public:
    bool contains(const RaceKey& k) const {
        return mem_.find(k) != mem_.end();
    }

    const RaceEntry* get(const RaceKey& k) const {
        auto it = mem_.find(k);
        if (it == mem_.end()) return nullptr;
        return &it->second;
    }

    RaceEntry* getMut(const RaceKey& k) {
        auto it = mem_.find(k);
        if (it == mem_.end()) return nullptr;
        return &it->second;
    }

    void put(const RaceKey& k, RaceEntry e) {
        mem_[k] = std::move(e);
    }

    const std::unordered_map<RaceKey, RaceEntry, RaceKeyHash>& raw() const {
        return mem_;
    }

private:
    std::unordered_map<RaceKey, RaceEntry, RaceKeyHash> mem_;
};

} // namespace runtime