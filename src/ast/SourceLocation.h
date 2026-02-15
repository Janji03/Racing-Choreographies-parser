#pragma once
#include <cstdint>
#include <string>

namespace ast {

// start: line 1-based, col 0-based (come ANTLR)
// end:   line 1-based, col 0-based (posizione del token di stop, caret-friendly)
struct SourcePos {
  uint32_t line = 0;
  uint32_t col  = 0;
};

struct SourceRange {
  std::string file;
  SourcePos start;
  SourcePos end;
};

} // namespace ast
