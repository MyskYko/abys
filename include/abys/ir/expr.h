#pragma once

#include <string>
#include <vector>

#include "abys/ir/type.h"

namespace abys::ir {

  struct ExprGraph {
    enum class Op {
      kUnknown,
      kConst,
      kInput,
      kLogicalNot,
      kAndReduce,
      kOrReduce,
      kXorReduce,
      kBitwiseNot,
      kAnd,
      kOr,
      kXor,
      kUnaryMinus,
      kAdd,
      kSub,
      kMul,
      kShl,
      kShr,
      kAshr,
      kEq,
      kNeq,
      kLt,
      kLe,
      kGt,
      kGe,
      kMux,
      kList,
      kCase,
      kConvert,
      kConcat,
      kMaskedAssign,
      kReverse,
      kRange,
      kArraySelect,
      kBothEdge,
    };

    struct Node {
      Op op = Op::kUnknown;
      SignalWidth width = 0;
      bool sign = false;
      std::vector<ExprId> operands;
    };
  
    struct Input {
      ExprId id;
      std::string name;
    };

    struct Constant {
      ExprId id;
      std::string value; // TODO: think about better structure
    };

    ExprId constant_zero = 0;
    ExprId constant_one = 1;
    std::vector<Node> nodes = {{Op::kConst, 1, false, {}}, {Op::kConst, 1, false, {}}};
    std::vector<Input> inputs;
    std::vector<Constant> constants = {{constant_zero, "1'b0"}, {constant_one, "1'b1"}};
  };

} // namespace abys::ir
