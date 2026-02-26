#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>

namespace runtime {

// Runtime value: int | bool
struct Value {
    enum class Kind { Int, Bool };

    Kind kind = Kind::Int;
    int  intValue = 0;
    bool boolValue = false;

    static Value makeInt(int v) {
        Value x;
        x.kind = Kind::Int;
        x.intValue = v;
        return x;
    }

    static Value makeBool(bool v) {
        Value x;
        x.kind = Kind::Bool;
        x.boolValue = v;
        return x;
    }

    std::string toString() const {
        if (kind == Kind::Int) return std::to_string(intValue);
        return boolValue ? "true" : "false";
    }
};

inline bool isBool(const Value& v) { return v.kind == Value::Kind::Bool; }
inline bool isInt(const Value& v)  { return v.kind == Value::Kind::Int;  }

inline bool asBool(const Value& v) {
    if (v.kind != Value::Kind::Bool) {
        throw std::runtime_error("Expected bool Value");
    }
    return v.boolValue;
}

inline int asInt(const Value& v) {
    if (v.kind != Value::Kind::Int) {
        throw std::runtime_error("Expected int Value");
    }
    return v.intValue;
}

} // namespace runtime