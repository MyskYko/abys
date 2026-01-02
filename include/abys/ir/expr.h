#pragma once

#include <string>
#include <vector>

#include "abys/ir/type.h"

namespace abys::ir {

struct ExprNode {
  enum class Op {
    kConst,
    kInput,
    kNot,
    kAndUnary,
    kOrUnary,
    kXorUnary,
    kAnd,
    kOr,
    kXor,
    kMux,
    kAdd,
    kSub,
    kMul,
    kShl,
    kShr,
    kEq,
    kNeq,
    kLt,
    kLe,
    kGt,
    kGe,
    kConvert,
    kMerge,
    kSelect,
  };

  Op op = Op::kConst;
  SignalWidth width = 0;
  bool sign = false;

  // for constants
  std::string value;

  // for signals
  PortIndex index = 0;

  // indices into ExprNode storage
  std::vector<ExprId> operands;
};

} // namespace abys::ir
