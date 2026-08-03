#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "abys/diagnostics.h"
#include "abys/ir/tig.h"
#include "abys/naming.h"

namespace abys {

struct ParseResult {
  bool ok = false;
  std::string message;
};

struct TigBuildResult {
  bool ok = false;
  std::string message;
  ir::Tig design;
};

/// Parse one or more SystemVerilog sources using slang.
ParseResult parse_systemverilog(const std::vector<std::string> &files, std::string_view top,
                                Diagnostics &diagnostics);

/// Build a TIG design from one or more SystemVerilog sources using slang.
TigBuildResult build_tig_from_systemverilog(const std::vector<std::string> &files,
                                            std::string_view top, Diagnostics &diagnostics,
                                            const NamingOptions &naming);

} // namespace abys
