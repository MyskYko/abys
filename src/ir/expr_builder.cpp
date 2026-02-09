#include <cassert>

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
  
ExprId ExprBuilder::create_mux(ExprId cond_id, ExprId then_id, ExprId else_id) {
  const ExprId id = create_node();
  auto &node = nodes_[id];
  node.op = ExprNode::Op::kMux;
  if(then_id != kInvalidExprId && else_id != kInvalidExprId) {
    const auto& then_node = nodes_[then_id];
    const auto& else_node = nodes_[else_id];
    assert(then_node.width == else_node.width);
    assert(then_node.sign == else_node.sign);
    node.width = then_node.width;
    node.sign = then_node.sign;
  } else if(then_id != kInvalidExprId) {
    const auto& then_node = nodes_[then_id];
    node.width = then_node.width;
    node.sign = then_node.sign;
  } else if(else_id != kInvalidExprId) {
    const auto& else_node = nodes_[else_id];
    node.width = else_node.width;
    node.sign = else_node.sign;
  } else {
    assert(false);
  }
  node.operands.push_back(cond_id);
  node.operands.push_back(then_id);
  node.operands.push_back(else_id);
  return id;
}

ExprId ExprBuilder::create_list(std::vector<ExprId> operands) {
  const ExprId id = create_node();
  auto &node = nodes_[id];
  node.op = ExprNode::Op::kList;
  node.operands = std::move(operands);
  return id;
}

ExprId ExprBuilder::create_case(ExprId case_id, const std::vector<ExprId>& case_values, const std::vector<ExprId>& data_ids, ExprId current_id) {
  const ExprId id = create_node();
  auto &node = nodes_[id];
  node.op = ExprNode::Op::kCase;
  node.operands.push_back(case_id);
  bool is_first = true;
  for (size_t i = 0; i < data_ids.size(); i++) {
    ExprId data_id = data_ids[i];
    if (data_id == kInvalidExprId) {
      data_id = current_id;
    }
    if (data_id != kInvalidExprId) {
      const auto& data_node = nodes_[data_id];
      if (is_first) {
        node.width = data_node.width;
        node.sign = data_node.sign;
        is_first = false;
      } else {
        assert(data_node.width == node.width);
        assert(data_node.sign == node.sign);
      }
    }
    if (i < case_values.size()) {
      node.operands.push_back(case_values[i]);
    }
    node.operands.push_back(data_id);
  }
  assert(!is_first);
  return id;
}

ExprId ExprBuilder::create_node() {
  const ExprId id = static_cast<ExprId>(nodes_.size());
  nodes_.emplace_back();
  return id;
}

} // namespace abys::ir
