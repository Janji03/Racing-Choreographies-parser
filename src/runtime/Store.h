#pragma once
#include <string>
#include <unordered_map>
#include <optional>

#include "runtime/Value.h"

namespace runtime {

// Store Σ: Process.Var -> Value
class Store final {
public:
    static std::string key(const std::string& process, const std::string& var) {
        return process + "." + var;
    }

    bool has(const std::string& process, const std::string& var) const {
        return map_.find(key(process, var)) != map_.end();
    }

    std::optional<Value> tryGet(const std::string& process, const std::string& var) const {
        auto it = map_.find(key(process, var));
        if (it == map_.end()) return std::nullopt;
        return it->second;
    }

    // Set Σ[p.x ↦ v]
    void set(const std::string& process, const std::string& var, const Value& v) {
        map_[key(process, var)] = v;
    }

    const std::unordered_map<std::string, Value>& raw() const { return map_; }

private:
    std::unordered_map<std::string, Value> map_;
};

} // namespace runtime