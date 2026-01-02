#pragma once

#include <cstdint>
#include <limits>

namespace abys::ir {

using PortIndex = uint32_t;
using SignalWidth = uint64_t;
using ExprId = uint32_t;
static constexpr ExprId kInvalidExprId = std::numeric_limits<ExprId>::max();

} // namespace abys::ir
