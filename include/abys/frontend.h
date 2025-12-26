#pragma once

#include <optional>
#include <string>
#include <vector>

namespace abys {

struct ParseResult {
  bool ok = false;
  std::string message;
};

/// Parse one or more SystemVerilog sources using slang.
ParseResult parse_systemverilog(const std::vector<std::string> &files,
                                const std::optional<std::string> &top);

} // namespace abys
