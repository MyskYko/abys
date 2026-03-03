#include <cassert>
#include <algorithm>
#include <bitset>

#include "abys/ir/expr_builder.h"

namespace abys::ir {

  ExprBuilder::ExprBuilder(ExprGraph &graph) : graph_(graph) {}

  ExprBuilder::ExprBuilder(const ExprBuilder &parent) : graph_(parent.graph_), name_map_(parent.name_map_) {}

  ExprId ExprBuilder::create_node() {
    const ExprId id = static_cast<ExprId>(graph_.nodes.size());
    graph_.nodes.emplace_back();
    return id;
  }
  ExprGraph::Node &ExprBuilder::get_node(ExprId id) {
    return graph_.nodes[id];
  }
  SignalWidth ExprBuilder::get_width(ExprId id) const {
    return graph_.nodes[id].width;
  }

  ExprId ExprBuilder::get_constant_zero() const {
    return graph_.constant_zero;
  }
  ExprId ExprBuilder::get_constant_one() const {
    return graph_.constant_one;
  }

  ExprId ExprBuilder::find_or_create_input(std::string name, SignalWidth width, bool sign) {
    const ExprId current = get_current_value(name);
    if (current != kInvalidExprId) {
      return current;
    }
    const auto it = graph_.inputs.find(name);
    if(it != graph_.inputs.end()) {
      return it->second;
    }
    const ExprId id = create_node();
    name_map_[name] = id;
    graph_.inputs.emplace(std::move(name), id);
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

  ExprId ExprBuilder::find_or_create_const(BitIndex index) {
    if (index == 0) {
      return get_constant_zero();
    }
    bool negative = false;
    if (index < 0) {
      negative = true;
      assert(index != std::numeric_limits<BitIndex>::min());
      index = -index;
    }
    std::bitset<SignalWidthBitSize> bits(index);
    std::string str = bits.to_string();
    size_t pos = str.find_last_not_of('0');
    assert(pos != std::string::npos);
    str.erase(pos + 1);
    const ExprId id = find_or_create_const(std::to_string(str.length()) + "'b" + str, str.length(), false);
    if (negative) {
      return create_unary_minus(id);
    }
    return id;
  }
  
  ExprId ExprBuilder::create_unary_reduce(ExprGraph::Op op, ExprId operand) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = op;
    node.width = 1;
    node.sign = false;
    node.operands.push_back(operand);
    return id;
  }
  ExprId ExprBuilder::create_logical_not(ExprId operand) {
    return create_unary_reduce(ExprGraph::Op::kLogicalNot, operand);
  }
  ExprId ExprBuilder::create_and_reduce(ExprId operand) {
    return create_unary_reduce(ExprGraph::Op::kAndReduce, operand);
  }
  ExprId ExprBuilder::create_or_reduce(ExprId operand) {
    return create_unary_reduce(ExprGraph::Op::kOrReduce, operand);
  }
  ExprId ExprBuilder::create_xor_reduce(ExprId operand) {
    return create_unary_reduce(ExprGraph::Op::kXorReduce, operand);
  }

  ExprId ExprBuilder::create_unary(ExprGraph::Op op, ExprId operand) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = op;
    const auto &operand_node = get_node(operand);
    node.width = operand_node.width;
    node.sign = operand_node.sign;
    node.operands.push_back(operand);
    return id;
  }
  ExprId ExprBuilder::create_bitwise_not(ExprId operand) {
    return create_unary(ExprGraph::Op::kBitwiseNot, operand);
  }
  
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
      node.sign = node.sign & operand_node.sign;
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

  ExprId ExprBuilder::create_unary_plus(ExprId a) {
    return a;
  }
  ExprId ExprBuilder::create_unary_minus(ExprId a) {
    return create_unary(ExprGraph::Op::kUnaryMinus, a);
  }

  ExprId ExprBuilder::create_binary(ExprGraph::Op op, ExprId a, ExprId b) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = op;
    const auto &a_node = get_node(a);
    const auto &b_node = get_node(b);
    node.width = std::max(a_node.width, b_node.width);
    node.sign = a_node.sign & b_node.sign;
    node.operands = {a, b};
    return id;
  }
  ExprId ExprBuilder::create_add(ExprId a, ExprId b) {
    return create_binary(ExprGraph::Op::kAdd, a, b);
  }
  ExprId ExprBuilder::create_sub(ExprId a, ExprId b) {
    return create_binary(ExprGraph::Op::kSub, a, b);
  }
  ExprId ExprBuilder::create_mul(ExprId a, ExprId b) {
    return create_binary(ExprGraph::Op::kMul, a, b);
  }
  ExprId ExprBuilder::create_div(ExprId a, ExprId b) {
    return create_binary(ExprGraph::Op::kDiv, a, b);
  }
  
  ExprId ExprBuilder::create_shift(ExprGraph::Op op, ExprId data, ExprId shamt) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = op;
    const auto &data_node = get_node(data);
    node.width = data_node.width;
    node.sign = data_node.sign;
    node.operands = {data, shamt};
    return id;
  }
  ExprId ExprBuilder::create_shl(ExprId data, ExprId shamt) {
    return create_shift(ExprGraph::Op::kShl, data, shamt);
  }
  ExprId ExprBuilder::create_shr(ExprId data, ExprId shamt) {
    return create_shift(ExprGraph::Op::kShr, data, shamt);
  }
  ExprId ExprBuilder::create_ashr(ExprId data, ExprId shamt) {
    return create_shift(ExprGraph::Op::kAshr, data, shamt);
  }

  ExprId ExprBuilder::create_compare(ExprGraph::Op op, ExprId a, ExprId b) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = op;
    node.width = 1;
    node.sign = false;
    node.operands = {a, b};
    return id;
  }
  ExprId ExprBuilder::create_eq(ExprId a, ExprId b) {
    return create_compare(ExprGraph::Op::kEq, a, b);
  }
  ExprId ExprBuilder::create_neq(ExprId a, ExprId b) {
    const ExprId id = create_compare(ExprGraph::Op::kEq, a, b);
    return create_logical_not(id);
  }
  ExprId ExprBuilder::create_lt(ExprId a, ExprId b) {
    return create_compare(ExprGraph::Op::kLt, a, b);
  }
  ExprId ExprBuilder::create_le(ExprId a, ExprId b) {
    return create_compare(ExprGraph::Op::kLe, a, b);
  }
  ExprId ExprBuilder::create_gt(ExprId a, ExprId b) {
    return create_lt(b, a);
  }
  ExprId ExprBuilder::create_ge(ExprId a, ExprId b) {
    return create_le(b, a);
  }

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

  ExprId ExprBuilder::create_concat(std::vector<ExprId> operands, bool sign) {
    assert(!operands.empty());
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kConcat;
    node.width = 0;
    node.sign = sign;
    for (ExprId operand : operands) {
      if (operand == kInvalidExprId) {
        node.width++;
      } else {
        const auto &operand_node = get_node(operand);
        node.width += operand_node.width;
      }
    }
    node.operands = std::move(operands);
    return id;
  }

  BitIndex ExprBuilder::normalize_index(BitIndex index, BitIndex msb, BitIndex lsb) {
    if (lsb == 0 && msb >= lsb) {
      return index;
    }
    if (msb >= lsb) {
      return index - lsb;
    }
    return lsb - index;
  }
  ExprId ExprBuilder::normalize_index_expr(ExprId index, BitIndex msb, BitIndex lsb) {
    if (lsb == 0 && msb >= lsb) {
      return index;
    }
    ExprId offset = find_or_create_const(lsb);
    if (msb >= lsb) {
      return create_sub(index, offset);
    }
    return create_sub(offset, index);
  }
  ExprId ExprBuilder::create_select(ExprId data, ExprId index, BitIndex msb, BitIndex lsb) {
    const ExprId pos = normalize_index_expr(index, msb, lsb);
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kRange;
    node.width = 1;
    node.sign = false;
    node.operands = {data, pos};
    return id;
  }
  ExprId ExprBuilder::create_reverse(ExprId data) {
    return create_unary(ExprGraph::Op::kReverse, data);
  }
  ExprId ExprBuilder::create_range(ExprId data, BitIndex left, BitIndex right, BitIndex msb, BitIndex lsb) {
    BitIndex left_pos = normalize_index(left, msb, lsb);
    BitIndex right_pos = normalize_index(right, msb, lsb);
    bool is_reverse = false;
    if (left_pos < right_pos) {
      is_reverse = true;
      std::swap(left_pos, right_pos);
    }
    const ExprId pos = find_or_create_const(left_pos);
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kRange;
    node.width = left_pos - right_pos + 1;
    node.sign = false;
    node.operands = {data, pos};
    if (is_reverse) {
      return create_reverse(id);
    }
    return id;
  }
  ExprId ExprBuilder::create_part_select(ExprId data, ExprId base, SignalWidth width, bool dir, BitIndex msb, BitIndex lsb) {
    ExprId left = base;
    if (dir) {
      assert(width > 0);
      assert(width <= static_cast<SignalWidth>(std::numeric_limits<BitIndex>::max()));
      const ExprId offset = find_or_create_const(width - 1);
      left = create_add(base, offset);
    }
    ExprId pos = normalize_index_expr(left, msb, lsb);
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kRange;
    node.width = width;
    node.sign = false;
    node.operands = {data, pos};
    if (dir) {
      return create_reverse(id);
    }
    return id;    
  }

  ExprId ExprBuilder::create_gather(std::vector<ExprId> operands) {
    assert(!operands.empty());
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kGather;
    node.width = operands.size();
    node.sign = false;
    node.operands = std::move(operands);
    return id;
  }
  ExprId ExprBuilder::create_masked_assign(ExprId current, ExprId next, ExprId base, ExprId slice_width, SignalWidth width, bool sign) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kMaskedAssign;
    node.width = width;
    node.sign = sign;
    assert(get_node(slice_width).op == ExprGraph::Op::kConst);
    node.operands = {current, next, base, slice_width};
    return id;
  }
  
  ExprId ExprBuilder::assign_select(ExprId data, ExprId index, std::string_view name, SignalWidth width, bool sign, BitIndex msb, BitIndex lsb) {
    ExprId current = get_current_value(name);
    ExprId pos = normalize_index_expr(index, msb, lsb);
    return create_masked_assign(current, data, pos, get_constant_one(), width, sign);
  }
  ExprId ExprBuilder::assign_range(ExprId data, BitIndex left, BitIndex right, std::string_view name, SignalWidth width, bool sign, BitIndex msb, BitIndex lsb) {
    ExprId current = get_current_value(name);
    BitIndex left_pos = normalize_index(left, msb, lsb);
    BitIndex right_pos = normalize_index(right, msb, lsb);
    if (left_pos < right_pos) {
      data = create_reverse(data);
      std::swap(left_pos, right_pos);
    }
    ExprId left_id = find_or_create_const(left_pos);
    ExprId width_id = find_or_create_const(left_pos - right_pos + 1);
    return create_masked_assign(current, data, left_id, width_id, width, sign);
  }
  ExprId ExprBuilder::assign_part_select(ExprId data, ExprId base, SignalWidth slice_width, bool dir, std::string_view name, SignalWidth width, bool sign, BitIndex msb, BitIndex lsb) {
    ExprId current = get_current_value(name);
    ExprId left = base;
    if (dir) {
      data = create_reverse(data);
      assert(slice_width > 0);
      const ExprId offset = find_or_create_const(slice_width - 1);
      left = create_add(base, offset);
    }
    ExprId pos = normalize_index_expr(left, msb, lsb);
    ExprId width_id = find_or_create_const(slice_width);
    return create_masked_assign(current, data, pos, width_id, width, sign);
  }

  ExprId ExprBuilder::create_call(const void *subr_ptr, std::string name, std::vector<ExprId> operands, SignalWidth width, bool sign) {
    // TODO: find_or_create?
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kCall;
    node.width = width;
    node.sign = sign;
    node.operands = std::move(operands);
    graph_.calls.emplace_back(ExprGraph::Call{id, subr_ptr, std::move(name)});
    return id;
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
  
  ExprId ExprBuilder::create_array_select(ExprId data, ExprId index, BitIndex msb, BitIndex lsb, SignalWidth width, bool sign) {
    const ExprId id = create_node();
    auto &node = get_node(id);
    node.op = ExprGraph::Op::kArraySelect;
    node.width = width;
    node.sign = sign;
    const ExprId pos = normalize_index_expr(index, msb, lsb);
    node.operands = {data, pos};
    return id;
  }

  ExprId ExprBuilder::get_current_value(std::string_view name) const {
    auto it = name_map_.find(name);
    if (it != name_map_.end()) {
      return it->second;
    }
    return kInvalidExprId;
  }
  void ExprBuilder::update_value(std::string name, ExprId id) {
    name_map_.insert_or_assign(std::move(name), id);
  }
  
} // namespace abys::ir
