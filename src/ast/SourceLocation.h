#pragma once
#include <cstdint>
#include <string>

namespace ast {

struct SourcePos {
  uint32_t line = 0;
  uint32_t col  = 0;
};

struct SourceRange {
  std::string file;
  SourcePos start;
  SourcePos end;
};

}