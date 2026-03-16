#pragma once
#include <string>
#include "ast/Ast.h"

namespace astjson {

// Serializza l'AST Program in JSON (string già pronta, indentata).
std::string serialize(const ast::Program& program);

} 