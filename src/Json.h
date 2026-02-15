#pragma once
#include <ostream>
#include <string>

namespace json {

inline std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                static const char* hex = "0123456789abcdef";
                out += "\\u00";
                out += hex[(c >> 4) & 0xF];
                out += hex[c & 0xF];
            } else {
                out += c;
            }
        }
    }
    return out;
}

class Writer {
public:
    explicit Writer(std::ostream& os, int indentSpaces = 2)
        : os_(os), indentSpaces_(indentSpaces) {}

    void beginObject() { writeIndent(); os_ << "{\n"; ++level_; first_ = true; }
    void endObject()   { os_ << "\n"; --level_; writeIndent(); os_ << "}"; first_ = false; }

    void beginArray(const char* key) { keyName(key); os_ << "[\n"; ++level_; first_ = true; }
    void endArray() { os_ << "\n"; --level_; writeIndent(); os_ << "]"; first_ = false; }

    // Start an array as value (no key) (for nested arrays)
    void arrayValueBegin() { elementSep(); writeIndent(); os_ << "[\n"; ++level_; first_ = true; }
    void arrayValueEnd()   { os_ << "\n"; --level_; writeIndent(); os_ << "]"; first_ = false; }

    void keyBool(const char* key, bool v) { keyName(key); os_ << (v ? "true" : "false"); }
    void keyInt(const char* key, int v)   { keyName(key); os_ << v; }
    void keyString(const char* key, const std::string& v) { keyName(key); os_ << "\"" << escape(v) << "\""; }

    // Allows embedding pre-serialized JSON (use carefully)
    void keyRaw(const char* key, const std::string& rawJson) { keyName(key); os_ << rawJson; }

    // array element helpers
    void elementString(const std::string& v) { elementSep(); writeIndent(); os_ << "\"" << escape(v) << "\""; }
    void elementInt(int v) { elementSep(); writeIndent(); os_ << v; }
    void elementBool(bool v) { elementSep(); writeIndent(); os_ << (v ? "true" : "false"); }

    void elementObjectBegin() { elementSep(); writeIndent(); os_ << "{\n"; ++level_; first_ = true; }
    void elementObjectEnd()   { os_ << "\n"; --level_; writeIndent(); os_ << "}"; first_ = false; }

private:
    std::ostream& os_;
    int indentSpaces_ = 2;
    int level_ = 0;
    bool first_ = true;

    void writeIndent() {
        for (int i = 0; i < level_ * indentSpaces_; ++i) os_ << ' ';
    }

    void elementSep() {
        if (!first_) os_ << ",\n";
        first_ = false;
    }

    void keyName(const char* key) {
        elementSep();
        writeIndent();
        os_ << "\"" << key << "\": ";
    }
};

} // namespace json
