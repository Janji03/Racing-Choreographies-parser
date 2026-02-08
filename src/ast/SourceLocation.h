#pragma once
#include <string>
#include <cstdint>

namespace ast {

struct SourcePos {
  uint32_t line = 0;   // 1-based
  uint32_t col  = 0;   // 0-based (come ANTLR)
};

struct SourceRange {
  std::string file;    // path o nome file
  SourcePos start;
  SourcePos end;       // end esclusivo o inclusivo: scegliamo “inclusivo caret-friendly”
};

} // namespace ast
