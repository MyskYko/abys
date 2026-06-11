#pragma once

#include <optional>
#include <string>
#include <vector>

#include "abys/ir/tig_builder.h"

namespace abys {

struct ParseResult {
  bool ok = false;
  std::string message;
};

/// Parse one or more SystemVerilog sources using slang.
ParseResult parse_systemverilog(const std::vector<std::string> &files,
                                const std::optional<std::string> &top);

/// Build a TIG design from one or more SystemVerilog sources using slang.
ir::TigBuildResult build_tig_from_systemverilog(const std::vector<std::string> &files,
                                                const std::optional<std::string> &top);

} // namespace abys
