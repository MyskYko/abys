#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace abys::ir {

using PortIndex = uint32_t;
using SignalWidth = uint64_t; // TODO: seems like 32 bit is slang max
using BitIndex = int64_t;
using ExprId = uint32_t;

enum class EdgeKind : uint8_t { kNone, kPosedge, kNegedge, kBothEdges };

constexpr int kSignalWidthBitSize = 64;

static constexpr ExprId kInvalidExprId = std::numeric_limits<ExprId>::max();

} // namespace abys::ir
