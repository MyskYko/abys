#pragma once

#include <string_view>

#include "abys/diagnostics.h"
#include "abys/ir/tig.h"

namespace slang::ast {
class RootSymbol;
}

namespace abys::frontend {

struct PragmaMap;

ir::Tig lower_slang_ast_to_ir(const slang::ast::RootSymbol &root, Diagnostics &diagnostics,
                              const PragmaMap &pragmas, std::string_view top = {});

} // namespace abys::frontend
