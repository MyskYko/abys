#pragma once

#include <string>
#include <cstdint>
#include <limits>

namespace abys::ir {

  using PortIndex = uint32_t;
  using SignalWidth = uint64_t;
  using BitIndex = int64_t;
  using ExprId = uint32_t;

  constexpr int SignalWidthBitSize = 64;

  static constexpr ExprId kInvalidExprId = std::numeric_limits<ExprId>::max();

} // namespace abys::ir
