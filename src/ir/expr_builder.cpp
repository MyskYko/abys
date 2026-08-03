#include <algorithm>
#include <bitset>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "abys/ir/expr_builder.h"

namespace abys::ir {

ExprBuilder::ExprBuilder(ExprGraph &graph) : graph_(graph) {}

ExprBuilder::ExprBuilder(const ExprBuilder &parent)
    : graph_(parent.graph_), name_map_(parent.name_map_) {}

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
bool ExprBuilder::get_sign(ExprId id) const {
  return graph_.nodes[id].sign;
}
bool ExprBuilder::is_sequence(ExprId id) const {
  assert(id != kInvalidExprId);
  return graph_.nodes[id].op == ExprGraph::Op::kSequence;
}

ExprId ExprBuilder::get_constant_zero() {
  return ExprGraph::constant_zero;
}
ExprId ExprBuilder::get_constant_one() {
  return ExprGraph::constant_one;
}

ExprId ExprBuilder::find_or_create_input(std::string name, SignalWidth width, bool sign) {
  const ExprId current = get_current_value(name);
  if (current != kInvalidExprId) {
    return current;
  }
  const auto it = graph_.inputs.find(name);
  if (it != graph_.inputs.end()) {
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
  if (width == 1 && !sign) {
    if (value == "1'b0") {
      return ExprGraph::constant_zero;
    }
    if (value == "1'b1") {
      return ExprGraph::constant_one;
    }
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
  ExprId id;
  if (index == 1) {
    id = get_constant_one();
  } else {
    std::bitset<kSignalWidthBitSize> bits(index);
    std::string str = bits.to_string();
    size_t pos = str.find_first_not_of('0');
    assert(pos != std::string::npos);
    str.erase(0, pos);
    id = find_or_create_const(std::to_string(str.length()) + "'b" + str, str.length(), false);
  }
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

ExprId ExprBuilder::create_logical_binary(ExprGraph::Op op, ExprId a, ExprId b) {
  const ExprId id = create_node();
  const auto &a_node = get_node(a);
  if (a_node.width > 1) {
    a = create_or_reduce(a);
  }
  const auto &b_node = get_node(b);
  if (b_node.width > 1) {
    b = create_or_reduce(b);
  }
  auto &node = get_node(id);
  node.op = op;
  node.width = 1;
  node.sign = false;
  node.operands = {a, b};
  return id;
}
ExprId ExprBuilder::create_logical_and(ExprId a, ExprId b) {
  return create_logical_binary(ExprGraph::Op::kAnd, a, b);
}
ExprId ExprBuilder::create_logical_or(ExprId a, ExprId b) {
  return create_logical_binary(ExprGraph::Op::kOr, a, b);
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
ExprId ExprBuilder::create_mod(ExprId a, ExprId b) {
  return create_binary(ExprGraph::Op::kMod, a, b);
}
ExprId ExprBuilder::create_pow(ExprId a, ExprId b) {
  return create_binary(ExprGraph::Op::kPow, a, b);
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
  if (then != kInvalidExprId && else_id != kInvalidExprId) {
    const auto &then_node = get_node(then);
    const auto &else_node = get_node(else_id);
    node.width = std::max(then_node.width, else_node.width);
    node.sign = then_node.sign & else_node.sign;
  } else if (then != kInvalidExprId) {
    const auto &then_node = get_node(then);
    node.width = then_node.width;
    node.sign = then_node.sign;
  } else if (else_id != kInvalidExprId) {
    const auto &else_node = get_node(else_id);
    node.width = else_node.width;
    node.sign = else_node.sign;
  } else {
    throw std::logic_error("Mux requires at least one valid data operand");
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
    eqs.reserve(case_value_node.operands.size());
    for (ExprId list_item : case_value_node.operands) {
      eqs.push_back(create_eq(selector, list_item));
    }
    return create_or(std::move(eqs));
  }
  return create_eq(selector, case_value);
}

ExprId ExprBuilder::create_case(ExprId selector, std::vector<ExprId> case_values,
                                std::vector<ExprId> data_ids) {
  assert(case_values.size() == data_ids.size() || case_values.size() + 1 == data_ids.size());
  const ExprId id = create_node();
  auto &node = get_node(id);
  node.op = ExprGraph::Op::kCase;
  node.operands.push_back(selector);
  bool is_first = true;
  for (size_t i = 0; i < data_ids.size(); ++i) {
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
      ++node.width;
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
  // TODO: size and sign index normalization using the selector, declared bounds, and array span.
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
ExprId ExprBuilder::create_simple_range(ExprId data, BitIndex left, BitIndex right, BitIndex msb,
                                        BitIndex lsb) {
  BitIndex left_pos = normalize_index(left, msb, lsb);
  BitIndex right_pos = normalize_index(right, msb, lsb);
  bool is_reverse = false;
  if (left_pos < right_pos) {
    is_reverse = true;
    std::swap(left_pos, right_pos);
  }
  const ExprId pos = find_or_create_const(right_pos);
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
ExprId ExprBuilder::create_range(ExprId data, ExprId base, SignalWidth width, bool sign) {
  assert(width > 0);
  const ExprId id = create_node();
  auto &node = get_node(id);
  node.op = ExprGraph::Op::kRange;
  node.width = width;
  node.sign = sign;
  node.operands = {data, base};
  return id;
}

ExprId ExprBuilder::create_unpacked_range(ExprId data, ExprId base, SignalWidth width) {
  assert(width > 0);
  const ExprId id = create_node();
  auto &node = get_node(id);
  node.op = ExprGraph::Op::kUnpackedRange;
  node.width = width;
  node.sign = false;
  node.operands = {data, base};
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
ExprId ExprBuilder::create_sequence(ExprId current, ExprId next) {
  assert(next != kInvalidExprId);
  std::vector<ExprId> operands;
  SignalWidth width;
  if (current != kInvalidExprId) {
    const auto &current_node = get_node(current);
    assert(current_node.op == ExprGraph::Op::kSequence);
    operands = current_node.operands;
    width = current_node.width;
    assert(get_node(next).width == width);
  } else {
    width = get_node(next).width;
  }
  operands.push_back(next);
  const ExprId id = create_node();
  auto &node = get_node(id);
  node.op = ExprGraph::Op::kSequence;
  node.width = width;
  node.sign = false;
  node.operands = std::move(operands);
  return id;
}
ExprId ExprBuilder::create_unpacked_assign(ExprId next, ExprId base, ExprId slice_width,
                                           SignalWidth width, bool sign) {
  const ExprId id = create_node();
  auto &node = get_node(id);
  node.op = ExprGraph::Op::kUnpackedAssign;
  node.width = width;
  node.sign = sign;
  assert(get_node(slice_width).op == ExprGraph::Op::kConst);
  node.operands = {next, base, slice_width};
  return id;
}
ExprId ExprBuilder::create_masked_assign(ExprId current, ExprId next, ExprId base,
                                         SignalWidth slice_width, SignalWidth width, bool sign) {
  assert(current != kInvalidExprId);
  assert(slice_width > 0);
  assert(slice_width <= static_cast<SignalWidth>(std::numeric_limits<BitIndex>::max()));
  const ExprId slice_width_id = find_or_create_const(static_cast<BitIndex>(slice_width));
  const ExprId id = create_node();
  auto &node = get_node(id);
  node.op = ExprGraph::Op::kMaskedAssign;
  node.width = width;
  node.sign = sign;
  node.operands = {current, next, base, slice_width_id};
  return id;
}

ExprId ExprBuilder::unpacked_assign_select(ExprId next, ExprId index, BitIndex msb, BitIndex lsb,
                                           SignalWidth width, bool sign) {
  const ExprId pos = normalize_index_expr(index, msb, lsb);
  return create_unpacked_assign(next, pos, get_constant_one(), width, sign);
}

ExprId ExprBuilder::unpacked_assign_range(ExprId next, BitIndex left, BitIndex right, BitIndex msb,
                                          BitIndex lsb, SignalWidth width, bool sign) {
  BitIndex left_pos = normalize_index(left, msb, lsb);
  BitIndex right_pos = normalize_index(right, msb, lsb);
  if (left_pos < right_pos) {
    next = create_reverse(next);
    std::swap(left_pos, right_pos);
  }
  const ExprId base_id = find_or_create_const(right_pos);
  const ExprId width_id = find_or_create_const(left_pos - right_pos + 1);
  return create_unpacked_assign(next, base_id, width_id, width, sign);
}

ExprId ExprBuilder::unpacked_assign_part_select(ExprId next, ExprId base, SignalWidth slice_width,
                                                bool dir, BitIndex msb, BitIndex lsb,
                                                SignalWidth width, bool sign) {
  assert(slice_width > 0);
  assert(slice_width <= static_cast<SignalWidth>(std::numeric_limits<BitIndex>::max()));
  ExprId low = base;
  if (msb < lsb) {
    next = create_reverse(next);
  }
  if (slice_width > 1) {
    const ExprId offset = find_or_create_const(static_cast<BitIndex>(slice_width - 1));
    if (dir) {
      if (msb < lsb) {
        low = create_add(base, offset);
      }
    } else {
      if (msb >= lsb) {
        low = create_sub(base, offset);
      }
    }
  }
  const ExprId base_id = normalize_index_expr(low, msb, lsb);
  const ExprId width_id = find_or_create_const(static_cast<BitIndex>(slice_width));
  return create_unpacked_assign(next, base_id, width_id, width, sign);
}

ExprId ExprBuilder::create_call(SubrId subr_id, std::string name, std::vector<ExprId> operands,
                                SignalWidth width, bool sign) {
  const ExprId id = create_node();
  auto &node = get_node(id);
  node.op = ExprGraph::Op::kCall;
  node.width = width;
  node.sign = sign;
  node.operands = std::move(operands);
  graph_.calls.emplace_back(ExprGraph::Call{id, subr_id, std::move(name)});
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

ExprId ExprBuilder::create_unpacked_select(ExprId data, ExprId index, BitIndex msb, BitIndex lsb,
                                           SignalWidth width, bool sign) {
  const ExprId pos = normalize_index_expr(index, msb, lsb);
  const ExprId id = create_node();
  auto &node = get_node(id);
  node.op = ExprGraph::Op::kUnpackedSelect;
  node.width = width;
  node.sign = sign;
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

void ExprBuilder::get_input_spec(ExprId id, ExprId &input_id, std::string &name, SignalWidth &width,
                                 bool &sign) const {
  input_id = id;
  const auto &node0 = graph_.nodes[input_id];
  if (node0.op == ExprGraph::Op::kConvert) {
    input_id = node0.operands[0];
  }
  const auto &node = graph_.nodes[input_id];
  if (node.op != ExprGraph::Op::kInput) {
    throw std::logic_error("Cannot obtain input spec of non-input expression");
  }
  bool found = false;
  for (const auto &kv : graph_.inputs) {
    if (kv.second == input_id) {
      name = kv.first;
      found = true;
      break;
    }
  }
  assert(found);
  width = node.width;
  sign = node.sign;
}

bool ExprBuilder::check_dependency_rec(ExprId id, ExprId target,
                                       std::unordered_set<ExprId> &visited) const {
  if (id == target) {
    return true;
  }
  if (id == kInvalidExprId) {
    return false;
  }
  if (!visited.insert(id).second) {
    return false;
  }
  const auto &node = graph_.nodes[id];
  for (ExprId operand : node.operands) {
    if (check_dependency_rec(operand, target, visited)) {
      return true;
    }
  }
  return false;
}

bool ExprBuilder::check_dependency(ExprId id, ExprId target) const {
  std::unordered_set<ExprId> visited;
  return check_dependency_rec(id, target, visited);
}

std::optional<int> ExprBuilder::try_evaluate(ExprId id) const {
  assert(id != kInvalidExprId);
  const auto &node = graph_.nodes[id];
  switch (node.op) {
  case ExprGraph::Op::kUnknown:
  case ExprGraph::Op::kInput:
    return std::nullopt;
  case ExprGraph::Op::kConst: {
    for (const auto &c : graph_.constants) {
      if (c.id != id) {
        continue;
      }
      const std::string &s = c.value;
      auto pos = s.find('\'');
      if (pos == std::string::npos) {
        return std::nullopt;
      }
      if (s[pos + 1] == 's') {
        ++pos;
      }
      if (s[pos + 1] != 'b') {
        return std::nullopt;
      }
      pos += 2;
      int value = 0;
      for (char ch : s.substr(pos)) {
        if (ch != '0' && ch != '1') {
          return std::nullopt;
        }
        value = (value << 1) | (ch - '0');
      }
      return value;
    }
    return std::nullopt;
  }
  case ExprGraph::Op::kLogicalNot:
    if (auto opr = try_evaluate(node.operands[0])) {
      return !*opr;
    }
    return std::nullopt;
  case ExprGraph::Op::kAndReduce: {
    const ExprId op_id = node.operands[0];
    const auto &op_node = graph_.nodes[op_id];
    auto opr = try_evaluate(op_id);
    if (!opr) {
      return std::nullopt;
    }
    for (SignalWidth i = 0; i < op_node.width; ++i) {
      if (((*opr >> i) & 1) == 0) {
        return 0;
      }
    }
    return 1;
  }
  case ExprGraph::Op::kOrReduce: {
    const ExprId op_id = node.operands[0];
    const auto &op_node = graph_.nodes[op_id];
    auto opr = try_evaluate(op_id);
    if (!opr) {
      return std::nullopt;
    }
    for (SignalWidth i = 0; i < op_node.width; ++i) {
      if ((*opr >> i) & 1) {
        return 1;
      }
    }
    return 0;
  }
  case ExprGraph::Op::kXorReduce: {
    const ExprId op_id = node.operands[0];
    const auto &op_node = graph_.nodes[op_id];
    auto opr = try_evaluate(op_id);
    if (!opr) {
      return std::nullopt;
    }
    int value = 0;
    for (SignalWidth i = 0; i < op_node.width; ++i) {
      value ^= (*opr >> i) & 1;
    }
    return value;
  }
  case ExprGraph::Op::kBitwiseNot:
    if (auto opr = try_evaluate(node.operands[0])) {
      return ~*opr;
    }
    return std::nullopt;
  case ExprGraph::Op::kAnd:
  case ExprGraph::Op::kOr:
  case ExprGraph::Op::kXor: {
    if (node.operands.empty()) {
      return std::nullopt;
    }
    auto value = try_evaluate(node.operands[0]);
    if (!value) {
      return std::nullopt;
    }
    for (size_t i = 1; i < node.operands.size(); ++i) {
      auto opr = try_evaluate(node.operands[i]);
      if (!opr) {
        return std::nullopt;
      }
      if (node.op == ExprGraph::Op::kAnd) {
        *value &= *opr;
      } else if (node.op == ExprGraph::Op::kOr) {
        *value |= *opr;
      } else {
        *value ^= *opr;
      }
    }
    return value;
  }
  case ExprGraph::Op::kUnaryMinus:
    if (auto opr = try_evaluate(node.operands[0])) {
      return -*opr;
    }
    return std::nullopt;
  case ExprGraph::Op::kAdd:
  case ExprGraph::Op::kSub:
  case ExprGraph::Op::kMul:
  case ExprGraph::Op::kDiv:
  case ExprGraph::Op::kMod:
  case ExprGraph::Op::kPow:
  case ExprGraph::Op::kShl:
  case ExprGraph::Op::kShr:
  case ExprGraph::Op::kEq:
  case ExprGraph::Op::kLt:
  case ExprGraph::Op::kLe: {
    auto lhs = try_evaluate(node.operands[0]);
    auto rhs = try_evaluate(node.operands[1]);
    if (!lhs || !rhs) {
      return std::nullopt;
    }
    switch (node.op) {
    case ExprGraph::Op::kAdd:
      return *lhs + *rhs;
    case ExprGraph::Op::kSub:
      return *lhs - *rhs;
    case ExprGraph::Op::kMul:
      return *lhs * *rhs;
    case ExprGraph::Op::kDiv:
      return *rhs == 0 ? std::nullopt : std::optional<int>(*lhs / *rhs);
    case ExprGraph::Op::kMod:
      return *rhs == 0 ? std::nullopt : std::optional<int>(*lhs % *rhs);
    case ExprGraph::Op::kPow:
      return static_cast<int>(std::pow(*lhs, *rhs));
    case ExprGraph::Op::kShl:
      return *lhs << *rhs;
    case ExprGraph::Op::kShr:
      return *lhs >> *rhs;
    case ExprGraph::Op::kEq:
      return *lhs == *rhs;
    case ExprGraph::Op::kLt:
      return *lhs < *rhs;
    case ExprGraph::Op::kLe:
      return *lhs <= *rhs;
    default:
      assert(0);
    }
  }
  case ExprGraph::Op::kAshr: {
    auto value = try_evaluate(node.operands[0]);
    auto shamt = try_evaluate(node.operands[1]);
    if (!value || !shamt) {
      return std::nullopt;
    }
    int shifted = *value;
    int mask = shifted & (1 << (node.width - 1));
    for (int i = 0; i < *shamt; ++i) {
      shifted = (shifted >> 1) | mask;
    }
    return shifted;
  }
  case ExprGraph::Op::kMux:
    if (auto cond = try_evaluate(node.operands[0])) {
      return try_evaluate(*cond ? node.operands[1] : node.operands[2]);
    }
    return std::nullopt;
  case ExprGraph::Op::kConvert:
    return try_evaluate(node.operands[0]);
    // TODO: extend these cases after constant evaluation supports structured arbitrary-width
    // values.
  case ExprGraph::Op::kList:
  case ExprGraph::Op::kCase:
  case ExprGraph::Op::kConcat:
  case ExprGraph::Op::kGather:
  case ExprGraph::Op::kSequence:
  case ExprGraph::Op::kUnpackedAssign:
  case ExprGraph::Op::kMaskedAssign:
  case ExprGraph::Op::kReverse:
  case ExprGraph::Op::kRange:
  case ExprGraph::Op::kUnpackedRange:
  case ExprGraph::Op::kUnpackedSelect:
  case ExprGraph::Op::kBothEdge:
  case ExprGraph::Op::kCall:
    return std::nullopt;
  }
  assert(0);
}

int ExprBuilder::evaluate(ExprId id) const {
  assert(id != kInvalidExprId);
  const auto &node = graph_.nodes[id];
  switch (node.op) {
  case ExprGraph::Op::kUnknown:
    assert(0);
    break;
  case ExprGraph::Op::kConst: {
    for (const auto &c : graph_.constants) {
      if (c.id != id) {
        continue;
      }
      const std::string &s = c.value;
      auto pos = s.find('\'');
      assert(pos != std::string::npos);
      assert(s[pos + 1] == 's' || s[pos + 1] == 'b');
      if (s[pos + 1] == 's') {
        ++pos;
      }
      assert(s[pos + 1] == 'b');
      pos += 2;
      const std::string digits = s.substr(pos);
      int value = 0;
      for (char ch : digits) {
        assert(ch == '0' || ch == '1');
        value = (value << 1) | (ch - '0');
      }
      return value;
    }
    assert(0);
    break;
  }
  case ExprGraph::Op::kInput:
    assert(0);
    break;
  case ExprGraph::Op::kLogicalNot:
    return !evaluate(node.operands[0]);
  case ExprGraph::Op::kAndReduce: {
    const ExprId op_id = node.operands[0];
    const auto &op_node = graph_.nodes[op_id];
    int opr = evaluate(op_id);
    for (SignalWidth i = 0; i < op_node.width; ++i) {
      if (((opr >> i) & 1) == 0) {
        return 0;
      }
    }
    return 1;
  }
  case ExprGraph::Op::kOrReduce: {
    const ExprId op_id = node.operands[0];
    const auto &op_node = graph_.nodes[op_id];
    int opr = evaluate(op_id);
    for (SignalWidth i = 0; i < op_node.width; ++i) {
      if ((opr >> i) & 1) {
        return 1;
      }
    }
    return 0;
  }
  case ExprGraph::Op::kXorReduce: {
    const ExprId op_id = node.operands[0];
    const auto &op_node = graph_.nodes[op_id];
    int opr = evaluate(op_id);
    int value = 0;
    for (SignalWidth i = 0; i < op_node.width; ++i) {
      value ^= (opr >> i) & 1;
    }
    return value;
  }
  case ExprGraph::Op::kBitwiseNot:
    return ~evaluate(node.operands[0]);
  case ExprGraph::Op::kAnd:
    return evaluate(node.operands[0]) & evaluate(node.operands[1]);
  case ExprGraph::Op::kOr:
    return evaluate(node.operands[0]) | evaluate(node.operands[1]);
  case ExprGraph::Op::kXor:
    return evaluate(node.operands[0]) ^ evaluate(node.operands[1]);
  case ExprGraph::Op::kUnaryMinus:
    return -evaluate(node.operands[0]);
  case ExprGraph::Op::kAdd:
    return evaluate(node.operands[0]) + evaluate(node.operands[1]);
  case ExprGraph::Op::kSub:
    return evaluate(node.operands[0]) - evaluate(node.operands[1]);
  case ExprGraph::Op::kMul:
    return evaluate(node.operands[0]) * evaluate(node.operands[1]);
  case ExprGraph::Op::kDiv:
    return evaluate(node.operands[0]) / evaluate(node.operands[1]);
  case ExprGraph::Op::kMod:
    return evaluate(node.operands[0]) % evaluate(node.operands[1]);
  case ExprGraph::Op::kPow:
    return static_cast<int>(std::pow(evaluate(node.operands[0]), evaluate(node.operands[1])));
  case ExprGraph::Op::kShl:
    return evaluate(node.operands[0]) << evaluate(node.operands[1]);
  case ExprGraph::Op::kShr:
    return evaluate(node.operands[0]) >> evaluate(node.operands[1]);
  case ExprGraph::Op::kAshr: {
    int value = evaluate(node.operands[0]);
    int shamt = evaluate(node.operands[1]);
    int mask = value & (1 << (node.width - 1));
    for (int i = 0; i < shamt; ++i) {
      value = (value >> 1) | mask;
    }
    return value;
  }
  case ExprGraph::Op::kEq:
    return evaluate(node.operands[0]) == evaluate(node.operands[1]);
  case ExprGraph::Op::kLt:
    return evaluate(node.operands[0]) < evaluate(node.operands[1]);
  case ExprGraph::Op::kLe:
    return evaluate(node.operands[0]) <= evaluate(node.operands[1]);
  case ExprGraph::Op::kMux:
    return evaluate(node.operands[0]) ? evaluate(node.operands[1]) : evaluate(node.operands[2]);
  case ExprGraph::Op::kList:
  case ExprGraph::Op::kCase:
    assert(0);
    break;
  case ExprGraph::Op::kConvert:
    return evaluate(node.operands[0]);
  case ExprGraph::Op::kConcat:
  case ExprGraph::Op::kGather:
  case ExprGraph::Op::kSequence:
  case ExprGraph::Op::kUnpackedAssign:
  case ExprGraph::Op::kMaskedAssign:
  case ExprGraph::Op::kReverse:
  case ExprGraph::Op::kRange:
  case ExprGraph::Op::kUnpackedRange:
  case ExprGraph::Op::kUnpackedSelect:
  case ExprGraph::Op::kBothEdge:
  case ExprGraph::Op::kCall:
    assert(0);
    break;
  }
  return -1;
}

} // namespace abys::ir
