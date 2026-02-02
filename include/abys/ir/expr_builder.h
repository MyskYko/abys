#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "abys/ir/expr.h"

namespace abys::ir {

class ExprBuilder {
public:
  explicit ExprBuilder(std::vector<ExprNode> &nodes);
  explicit ExprBuilder(std::vector<ExprNode> &nodes, std::vector<ExprInput> &inputs, std::unordered_map<std::string, ExprId> &name_map);

  ExprId find_or_create_input(std::string name, SignalWidth width, bool sign);

  ExprId create_const(std::string value, SignalWidth width, bool sign);
  ExprId create_unary(ExprNode::Op op, ExprId operand, SignalWidth width, bool sign);
  ExprId create_binary(ExprNode::Op op, ExprId lhs, ExprId rhs, SignalWidth width, bool sign);
  ExprId create_ternary(ExprNode::Op op, ExprId a, ExprId b, ExprId c, SignalWidth width,
                        bool sign);
  ExprId create_nary(ExprNode::Op op, std::vector<ExprId> operands, SignalWidth width, bool sign);
  ExprId create_mux(ExprId a, ExprId b, ExprId c);
  ExprId create_convert(ExprId operand, SignalWidth width, bool sign);

  template<typename Func>
  void for_each_input(Func &&func);    

private:
  ExprId create_node();

  std::vector<ExprInput> owned_inputs_;
  std::unordered_map<std::string, ExprId> owned_name_map_;
  
  std::vector<ExprNode> &nodes_;
  std::vector<ExprInput> &inputs_;
  std::unordered_map<std::string, ExprId> &name_map_;
};

template<typename Func>
void ExprBuilder::for_each_input(Func &&func) {
  for (auto const &input : inputs_) {
    auto const &node = nodes_[input.id];
    func(input.name, node.width, node.sign);
  }
}

} // namespace abys::ir
