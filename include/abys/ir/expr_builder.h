#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "abys/ir/expr.h"

namespace abys::ir {

  class ExprBuilder {
  public:
    explicit ExprBuilder(ExprGraph &graph);
    explicit ExprBuilder(ExprGraph &graph, std::unordered_map<std::string, ExprId> &name_map);

    ExprId get_constant_zero() const;
    ExprId get_constant_one() const;

    ExprGraph::Node &get_node(ExprId id);
    
    ExprId find_or_create_input(std::string name, SignalWidth width, bool sign);
    ExprId find_or_create_const(std::string value, SignalWidth width, bool sign);
  
    ExprId create_logical_not(ExprId operand);
    ExprId create_bitwise_not(ExprId operand);
    ExprId create_and_reduce(ExprId operand);
    ExprId create_or_reduce(ExprId operand);
    ExprId create_xor_reduce(ExprId operand);
  
    ExprId create_and(std::vector<ExprId> operands);
    ExprId create_or(std::vector<ExprId> operands);
    ExprId create_xor(std::vector<ExprId> operands);

    ExprId create_unary_plus(ExprId a); // nop
    ExprId create_unary_minus(ExprId a);
    ExprId create_add(ExprId a, ExprId b);
    ExprId create_sub(ExprId a, ExprId b);
    ExprId create_mul(ExprId a, ExprId b);
  
    ExprId create_shl(ExprId data, ExprId shamt);
    ExprId create_shr(ExprId data, ExprId shamt);
    ExprId create_ashr(ExprId data, ExprId shamt);

    ExprId create_eq(ExprId a, ExprId b);
    ExprId create_neq(ExprId a, ExprId b); // map to logical_not(eq)
    ExprId create_lt(ExprId a, ExprId b);
    ExprId create_le(ExprId a, ExprId b);
    ExprId create_gt(ExprId a, ExprId b); // map to logical_not(le)
    ExprId create_ge(ExprId a, ExprId b); // map to logical_not(lt)

    ExprId create_mux(ExprId cond, ExprId then, ExprId else_id); // cond ? then : else;
  
    ExprId create_list(std::vector<ExprId> operands);
    ExprId create_match(ExprId selector, ExprId case_value); // case_value can be a list
    ExprId create_case(ExprId selector, std::vector<ExprId> case_values, std::vector<ExprId> data_ids);
  
    ExprId create_convert(ExprId a, SignalWidth width, bool sign);
  
    ExprId create_concat(std::vector<ExprId> operands);
    ExprId create_select(ExprId data, ExprId index);
    ExprId create_range(ExprId data, SignalWidth left, SignalWidth right); // maps to Range, creating operands[1] for left, storing width as abs(left - right), and storing left < right as ascending (sign is false)
    ExprId create_part_select(ExprId data, ExprId base, SignalWidth width, bool dir); // maps to Range

    ExprId create_both_edge(ExprId operand);

    template<typename Func>
    void for_each_input(Func &&func) {
      for (auto const &input : graph_.inputs) {
        auto const &node = graph_.nodes[input.id];
        func(input.name, node.width, node.sign);
      }
    }

  private:
    ExprGraph &graph_;
    std::unordered_map<std::string, ExprId> owned_name_map_;
    std::unordered_map<std::string, ExprId> &name_map_;
  
    ExprId create_node();
    
    ExprId create_nary(ExprGraph::Op op, std::vector<ExprId> operands);
  };

} // namespace abys::ir
