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
      kBitwiseNot,
      kAndReduce,
      kOrReduce,
      kXorReduce,
      kAnd,
      kOr,
      kXor,
      kUnaryPlus,
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
      kSelect,
      kRange,
      kBothEdge,
    };

    struct Node {
      Op op = Op::kUnknown;
      SignalWidth width = 0;
      bool sign = false;
      bool ascending = false; // for range
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
    std::vector<Node> nodes = {{Op::kConst, 1, false, false, {}}, {Op::kConst, 1, false, false, {}}};
    std::vector<Input> inputs;
    std::vector<Constant> constants = {{constant_zero, "1'b0"}, {constant_one, "1'b1"}};
  };

} // namespace abys::ir
