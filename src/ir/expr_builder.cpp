#include "abys/ir/expr_builder.h"

namespace abys::ir {

ExprBuilder::ExprBuilder(std::vector<ExprNode> &nodes) : nodes_(nodes), inputs_(owned_inputs_), name_map_(owned_name_map_) {}

ExprBuilder::ExprBuilder(std::vector<ExprNode> &nodes, std::vector<ExprInput> &inputs, std::unordered_map<std::string, ExprId> &name_map) : nodes_(nodes), inputs_(inputs), name_map_(name_map) {}

ExprId ExprBuilder::find_or_create_input(std::string name, SignalWidth width, bool sign) {
  auto it = name_map_.find(name);
  if(it != name_map_.end()) {
    return it->second;
  }
  const ExprId id = create_node();
  const PortIndex port_idx = static_cast<PortIndex>(inputs_.size());
  name_map_[name] = id;
  inputs_.push_back({id, std::move(name)});
  auto &node = nodes_[id];
  node.op = ExprNode::Op::kInput;
  node.width = width;
  node.sign = sign;
  node.index = port_idx;
  return id;
}

ExprId ExprBuilder::create_convert(ExprId operand, SignalWidth width, bool sign) {
  return create_unary(ExprNode::Op::kConvert, operand, width, sign);
}

ExprId ExprBuilder::create_const(std::string value, SignalWidth width, bool sign) {
  const ExprId id = create_node();
  auto &node = nodes_[id];
  node.op = ExprNode::Op::kConst;
  node.width = width;
  node.sign = sign;
  node.value = std::move(value);
  return id;
}

ExprId ExprBuilder::create_unary(ExprNode::Op op, ExprId operand, SignalWidth width, bool sign) {
  const ExprId id = create_node();
  auto &node = nodes_[id];
  node.op = op;
  node.width = width;
  node.sign = sign;
  node.operands.push_back(operand);
  return id;
}

ExprId ExprBuilder::create_binary(ExprNode::Op op, ExprId lhs, ExprId rhs, SignalWidth width,
                                  bool sign) {
  const ExprId id = create_node();
  auto &node = nodes_[id];
  node.op = op;
  node.width = width;
  node.sign = sign;
  node.operands.push_back(lhs);
  node.operands.push_back(rhs);
  return id;
}

ExprId ExprBuilder::create_ternary(ExprNode::Op op, ExprId a, ExprId b, ExprId c,
                                   SignalWidth width, bool sign) {
  const ExprId id = create_node();
  auto &node = nodes_[id];
  node.op = op;
  node.width = width;
  node.sign = sign;
  node.operands.push_back(a);
  node.operands.push_back(b);
  node.operands.push_back(c);
  return id;
}

ExprId ExprBuilder::create_nary(ExprNode::Op op, std::vector<ExprId> operands,
                                SignalWidth width, bool sign) {
  const ExprId id = create_node();
  auto &node = nodes_[id];
  node.op = op;
  node.width = width;
  node.sign = sign;
  node.operands = std::move(operands);
  return id;
}

ExprId ExprBuilder::create_node() {
  const ExprId id = static_cast<ExprId>(nodes_.size());
  nodes_.emplace_back();
  return id;
}

} // namespace abys::ir
