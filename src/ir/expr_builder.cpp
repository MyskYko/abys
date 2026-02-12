#include <cassert>

#include "abys/ir/expr_builder.h"

namespace abys::ir {

  ExprBuilder::ExprBuilder(ExprGraph &graph) : graph_(graph), name_map_(owned_name_map_) {}

  ExprBuilder::ExprBuilder(ExprGraph &graph, std::unordered_map<std::string, ExprId> &name_map) : graph_(graph), name_map_(name_map) {}

  ExprId ExprBuilder::create_node() {
    const ExprId id = static_cast<ExprId>(graph_.nodes.size());
    graph_.nodes.emplace_back();
    return id;
  }
  ExprGraph::Node &ExprBuilder::get_node(ExprId id) {
    return graph_.nodes[id];
  }

  ExprId ExprBuilder::get_constant_zero() const {
    return graph_.constant_zero;
  }
  ExprId ExprBuilder::get_constant_one() const {
    return graph_.constant_one;
  }

  ExprId ExprBuilder::find_or_create_input(std::string name, SignalWidth width, bool sign) {
    const auto it = name_map_.find(name);
    if(it != name_map_.end()) {
      return it->second;
    }
    const ExprId id = create_node();
    name_map_[name] = id;
    graph_.inputs.emplace_back(ExprGraph::Input{id, std::move(name)});
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kInput;
    node.width = width;
    node.sign = sign;
    return id;
  }

  ExprId ExprBuilder::find_or_create_const(std::string value, SignalWidth width, bool sign) {
    if (width == 1 && sign == false) {
      if (value == "1'b0") {
      return graph_.constant_zero;
      }
      if (value == "1'b1") {
        return graph_.constant_one;
      }
      assert(false);
    }
    const ExprId id = create_node();
    graph_.constants.emplace_back(ExprGraph::Constant{id, std::move(value)});
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kConst;
    node.width = width;
    node.sign = sign;
    return id;
  }
  
  ExprId ExprBuilder::create_logical_not(ExprId operand) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kLogicalNot;
    node.width = 1;
    node.sign = false;
    node.operands.push_back(operand);
    return id;
  }
  ExprId ExprBuilder::create_bitwise_not(ExprId operand) { return kInvalidExprId; }
  ExprId ExprBuilder::create_and_reduce(ExprId operand) { return kInvalidExprId; }
  ExprId ExprBuilder::create_or_reduce(ExprId operand) { return kInvalidExprId; }
  ExprId ExprBuilder::create_xor_reduce(ExprId operand) { return kInvalidExprId; }

  ExprId ExprBuilder::create_nary(ExprGraph::Op op, std::vector<ExprId> operands) {
    assert(!operands.empty());
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = op;
    node.width = 0;
    node.sign = true;
    for (ExprId operand : operands) {
      const auto &operand_node = get_node(operand);
      node.width = std::max(node.width, operand_node.width);
      node.sign &= operand_node.sign;
    }
    node.operands = std::move(operands);
    return id;
  }
  ExprId ExprBuilder::create_and(std::vector<ExprId> operands) {
    return create_nary(ExprGraph::Op::kAnd, std::move(operands));
  }
  ExprId ExprBuilder::create_or(std::vector<ExprId> operands) {
    return create_nary(ExprGraph::Op::kOr, std::move(operands));
  }
  ExprId ExprBuilder::create_xor(std::vector<ExprId> operands) {
    return create_nary(ExprGraph::Op::kXor, std::move(operands));
  }

  ExprId ExprBuilder::create_unary_plus(ExprId a) { return kInvalidExprId; } // nop
  ExprId ExprBuilder::create_unary_minus(ExprId a) { return kInvalidExprId; }
  ExprId ExprBuilder::create_add(ExprId a, ExprId b) { return kInvalidExprId; }
  ExprId ExprBuilder::create_sub(ExprId a, ExprId b) { return kInvalidExprId; }
  ExprId ExprBuilder::create_mul(ExprId a, ExprId b) { return kInvalidExprId; }
  
  ExprId ExprBuilder::create_shl(ExprId data, ExprId shamt) { return kInvalidExprId; }
  ExprId ExprBuilder::create_shr(ExprId data, ExprId shamt) { return kInvalidExprId; }
  ExprId ExprBuilder::create_ashr(ExprId data, ExprId shamt) { return kInvalidExprId; }

  ExprId ExprBuilder::create_eq(ExprId a, ExprId b) { return kInvalidExprId; }
  ExprId ExprBuilder::create_neq(ExprId a, ExprId b) { return kInvalidExprId; } // map to logical_not(eq)
  ExprId ExprBuilder::create_lt(ExprId a, ExprId b) { return kInvalidExprId; }
  ExprId ExprBuilder::create_le(ExprId a, ExprId b) { return kInvalidExprId; }
  ExprId ExprBuilder::create_gt(ExprId a, ExprId b) { return kInvalidExprId; } // map to logical_not(le)
  ExprId ExprBuilder::create_ge(ExprId a, ExprId b) { return kInvalidExprId; } // map to logical_not(lt)

  ExprId ExprBuilder::create_mux(ExprId cond, ExprId then, ExprId else_id) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kMux;
    if(then != kInvalidExprId && else_id != kInvalidExprId) {
      const auto &then_node = get_node(then);
      const auto &else_node = get_node(else_id);
      node.width = std::max(then_node.width, else_node.width);
      node.sign = then_node.sign & else_node.sign;
    } else if(then != kInvalidExprId) {
      const auto &then_node = get_node(then);
      node.width = then_node.width;
      node.sign = then_node.sign;
    } else if(else_id != kInvalidExprId) {
      const auto &else_node = get_node(else_id);
      node.width = else_node.width;
      node.sign = else_node.sign;
    } else {
      assert(false);
    }
    node.operands = {cond, then, else_id};
    return id;
  }
  
  ExprId ExprBuilder::create_list(std::vector<ExprId> operands) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kList;
    node.operands = std::move(operands);
    return id;
  }
  
  ExprId ExprBuilder::create_match(ExprId selector, ExprId case_value) {
    const auto &case_value_node = get_node(case_value);
    if (case_value_node.op == ExprGraph::Op::kList) {
      std::vector<ExprId> eqs;
      for (ExprId list_item : case_value_node.operands) {
        eqs.push_back(create_eq(selector, list_item));
      }
      return create_or(std::move(eqs));
    }
    return create_eq(selector, case_value);
  }
  
  ExprId ExprBuilder::create_case(ExprId selector, std::vector<ExprId> case_values, std::vector<ExprId> data_ids) {
    assert(case_values.size() == data_ids.size() || case_values.size() + 1 == data_ids.size());
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kCase;
    node.operands.push_back(selector);
    bool is_first = true;
    for (size_t i = 0; i < data_ids.size(); i++) {
      if (i < case_values.size()) {
        node.operands.push_back(case_values[i]);
      }
      node.operands.push_back(data_ids[i]);
      if (data_ids[i] != kInvalidExprId) {
        const auto &data_node = get_node(data_ids[i]);
        if (is_first) {
          is_first = false;
          node.width = data_node.width;
          node.sign = data_node.sign;
        } else {
          assert(data_node.width == node.width);
          assert(data_node.sign == node.sign);
        }
      }
    }
    assert(!is_first);
    return id;
  }

  ExprId ExprBuilder::create_convert(ExprId operand, SignalWidth width, bool sign) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kConvert;
    node.width = width;
    node.sign = sign;
    node.operands.push_back(operand);
    return id;
  }

  ExprId ExprBuilder::create_concat(std::vector<ExprId> operands) {
    return kInvalidExprId;
  }
  ExprId ExprBuilder::create_select(ExprId data, ExprId index) {
    return kInvalidExprId;
  }
  ExprId ExprBuilder::create_range(ExprId data, SignalWidth left, SignalWidth right) {
    return kInvalidExprId;
  }
  ExprId ExprBuilder::create_part_select(ExprId data, ExprId base, SignalWidth width, bool dir) {
    return kInvalidExprId;
  }

  ExprId ExprBuilder::create_both_edge(ExprId operand) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kBothEdge;
    node.width = 1;
    node.sign = false;
    node.operands.push_back(operand);
    return id;
  }
  
} // namespace abys::ir
