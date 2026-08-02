#pragma once

namespace slang::ast {
class RootSymbol;
}

namespace abys::ir {
class TigBuilder;
}

namespace abys::frontend {

struct PragmaMap;

void lower_slang_ast_to_ir(const slang::ast::RootSymbol &root, ir::TigBuilder &builder,
                           const PragmaMap &pragmas);
void lower_slang_ast_to_ir(const slang::ast::RootSymbol &root, ir::TigBuilder &builder);

} // namespace abys::frontend
