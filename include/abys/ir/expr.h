#pragma once

#include <string>
#include <unordered_map>
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
    kDiv, // TODO: handle unsynthesizable if not constant
    kMod, // TODO: handle unsynthesizable if not constant
    kPow, // TODO: handle unsynthesizable if not constant
    kShl,
    kShr,
    kAshr,
    kEq,
    kLt,
    kLe,
    kMux,
    kList,
    kCase,
    kConvert,
    kConcat,
    kGather,
    kSequence,
    kUnpackedAssign,
    kMaskedAssign,
    kReverse,
    kRange,
    kUnpackedRange,
    kUnpackedSelect,
    kBothEdge,
    kCall,
  };

  struct Node {
    Op op = Op::kUnknown;
    SignalWidth width = 0;
    bool sign = false;
    std::vector<ExprId> operands;
  };

  struct Constant {
    ExprId id;
    std::string value; // TODO: think about better structure
  };

  // TODO: probably a better way is to hold map from subr to subr_id in visitor and use subr_id here
  struct Call {
    ExprId id;
    const void *subr_ptr;
    std::string name; // to debug
  };

  static constexpr ExprId constant_zero = 0;
  static constexpr ExprId constant_one = 1;
  std::vector<Node> nodes = {{Op::kConst, 1, false, {}}, {Op::kConst, 1, false, {}}};
  std::unordered_map<std::string, ExprId> inputs;
  std::vector<Constant> constants = {{constant_zero, "1'b0"}, {constant_one, "1'b1"}};
  std::vector<Call> calls;
};

} // namespace abys::ir
