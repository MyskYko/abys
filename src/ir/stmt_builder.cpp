#include <cassert>

#include "abys/ir/stmt_builder.h"

namespace abys::ir {

  StmtBuilder::StmtBuilder(ExprGraph &expr_graph): expr_graph_(expr_graph) {
    contexts_.emplace_back();
  }

  ExprBuilder StmtBuilder::make_expr_builder() {
    return ExprBuilder(expr_graph_, current_values());
  }

  std::unordered_map<std::string, ExprId> &StmtBuilder::current_values() {
    return contexts_.back().current_values;
  }

  std::vector<std::string> &StmtBuilder::output_names() {
    return contexts_.back().output_names;
  }

  std::vector<bool> &StmtBuilder::output_nonblocking() {
    return contexts_.back().output_nonblocking;
  }

  std::vector<ExprId> &StmtBuilder::output_ids() {
    return contexts_.back().output_ids;
  }

  size_t StmtBuilder::get_context_stack_index() const {
    return context_stack_.size();
  }

  void StmtBuilder::set_comb() {
    policy_ = Policy::Comb;
  }

  void StmtBuilder::set_latch() {
    policy_ = Policy::Latch;
  }

  void StmtBuilder::set_comb_or_latch() {
    policy_ = Policy::CombOrLatch;
  }

  void StmtBuilder::set_ff() {
    policy_ = Policy::Ff;
  }

  bool StmtBuilder::is_root_context() const {
    return contexts_.size() == 1;
  }
  
  bool StmtBuilder::is_comb() const {
    return policy_ == Policy::Comb;
  }
  bool StmtBuilder::is_ff() const {
    return policy_ == Policy::Ff;
  }
  bool StmtBuilder::is_undecided() const {
    return policy_ == Policy::Undecided;
  }

  bool StmtBuilder::has_timing() const {
    return !timing_events_.empty();
  }

  void StmtBuilder::add_timing(ExprId expr_id, ExprId iff_id, bool posedge, bool negedge) {
    EdgeKind edge_kind = EdgeKind::kNone;
    if (posedge && negedge) {
      edge_kind = EdgeKind::kBothEdges;
    } else if (posedge) {
      edge_kind = EdgeKind::kPosedge;
    } else if (negedge) {
      edge_kind = EdgeKind::kNegedge;
    }
    if (edge_kind == EdgeKind::kNone) {
      if (!is_undecided()) {
        throw std::logic_error("Level-sensitive timing in non-undecided block");
      }
      set_comb_or_latch();
    } else {
      if (!is_undecided() && !is_ff()) {
        throw std::logic_error("Edge timing in comb/latch block");
      }
      set_ff();
    }
    timing_events_.push_back({edge_kind, expr_id, iff_id});
  }
  
  void StmtBuilder::create_context() {
    Context& ctx = contexts_.back();
    contexts_.emplace_back();
    contexts_.back().current_values = ctx.current_values;
  }

  void StmtBuilder::stack_context() {
    assert(contexts_.size() > 1);
    context_stack_.push_back(std::move(contexts_.back()));
    contexts_.pop_back();    
  }

  void StmtBuilder::transfer_output(const Context& from, size_t i, ExprId expr_id) {
    if (!from.output_nonblocking[i]) {
      current_values()[from.output_names[i]] = expr_id;
    }
    output_names().push_back(from.output_names[i]);
    output_nonblocking().push_back(from.output_nonblocking[i]);
    output_ids().push_back(expr_id);
  }
  
  void StmtBuilder::merge_context() {
    assert(contexts_.size() > 1);
    Context child = std::move(contexts_.back());
    contexts_.pop_back();
    // append outputs if not local & update their current values
    for (size_t i = 0; i < child.output_names.size(); i++) {
      if (child.local_names.count(child.output_names[i])) {
	continue;
      }
      transfer_output(child, i, child.output_ids[i]);
    }
  }

  void StmtBuilder::merge_conditional(ExprId cond_id) {
    assert(context_stack_.size() > 1);
    Context else_ctx = std::move(context_stack_.back());
    context_stack_.pop_back();
    Context then_ctx = std::move(context_stack_.back());
    context_stack_.pop_back();
    // compute shared outputs
    std::unordered_set<std::string> then_outputs;
    for (size_t i = 0; i < then_ctx.output_names.size(); i++) {
      then_outputs.insert(then_ctx.output_names[i]);
    }
    std::unordered_map<std::string, size_t> shared_output_to_else_index;
    for (size_t i = 0; i < else_ctx.output_names.size(); i++) {
      if (then_outputs.count(else_ctx.output_names[i])) {
	shared_output_to_else_index[else_ctx.output_names[i]] = i;
      }
    }
    // append then/shared outputs if not local & update their current values
    ExprBuilder expr_builder = make_expr_builder();
    for (size_t i = 0; i < then_ctx.output_names.size(); i++) {
      assert (then_ctx.local_names.empty()); // assuming locals can only be declared in block, which removes local on merge
      const std::string& name = then_ctx.output_names[i];
      auto it = shared_output_to_else_index.find(name);
      ExprId new_id = kInvalidExprId;
      if (it != shared_output_to_else_index.end()) {
	// shared
	ExprId then_id = then_ctx.output_ids[i];
	ExprId else_id = else_ctx.output_ids[it->second];
	new_id = expr_builder.create_mux(cond_id, then_id, else_id);
	if (then_ctx.output_nonblocking[i] != else_ctx.output_nonblocking[it->second]) {
	  throw std::logic_error("Mixed blocking/nonblocking assignments to " + name);
	}
      } else {
	// not shared
	ExprId then_id = then_ctx.output_ids[i];
	ExprId else_id = kInvalidExprId;
	auto current_it = current_values().find(name);
	if (current_it != current_values().end()) {
	  else_id = current_it->second;
	}
	new_id = expr_builder.create_mux(cond_id, then_id, else_id);
      }
      assert(new_id != kInvalidExprId);
      transfer_output(then_ctx, i, new_id);
    }
    // append else/non-shared outputs if not local & update their current values
    for (size_t i = 0; i < else_ctx.output_names.size(); i++) {
      assert (else_ctx.local_names.empty());
      const std::string& name = else_ctx.output_names[i];
      if (shared_output_to_else_index.count(name)) {
	continue;
      }
      // not shared
      ExprId then_id = kInvalidExprId;
      auto current_it = current_values().find(name);
      if (current_it != current_values().end()) {
	then_id = current_it->second;
      }
      ExprId else_id = else_ctx.output_ids[i];
      ExprId new_id = expr_builder.create_mux(cond_id, then_id, else_id);
      transfer_output(else_ctx, i, new_id);
    }
  }

  void StmtBuilder::merge_case(ExprId case_id, const std::vector<ExprId>& case_values, size_t stack_index) {
    size_t output_count = 0;
    std::unordered_map<std::string, size_t> output_map;
    std::vector<std::vector<ExprId>> case_output_ids;
    std::vector<std::vector<bool>> case_output_nonblocking;
    for (size_t j = 0; j < context_stack_.size() - stack_index; j++) {
      const Context& ctx = context_stack_[j + stack_index];
      for (size_t i = 0; i < ctx.output_names.size(); i++) {
	const std::string& name = ctx.output_names[i];
	auto it = output_map.find(name);
	size_t output_index;
	if (it == output_map.end()) {
	  output_index = output_count;
	  output_map[name] = output_count;
	  output_count++;
	  case_output_nonblocking.resize(output_count);
	  case_output_ids.resize(output_count);
	} else {
	  output_index = it->second;
	}
	case_output_ids[output_index].resize(j + 1, kInvalidExprId);
	case_output_ids[output_index][j] = ctx.output_ids[i];
	case_output_nonblocking[output_index].resize(j + 1, false);
	case_output_nonblocking[output_index][j] = ctx.output_nonblocking[i];
      }
    }
    ExprBuilder expr_builder = make_expr_builder();
    for (const auto &entry : output_map) {
      ExprId current_id = kInvalidExprId;
      auto current_it = current_values().find(entry.first);
      if (current_it != current_values().end()) {
        current_id = current_it->second;
      }
      bool is_first = true;
      bool nonblocking;
      for (size_t j = 0; j < case_output_ids[entry.second].size(); j++) {
	if (case_output_ids[entry.second][j] != kInvalidExprId) {
	  if (is_first) {
	    nonblocking = case_output_nonblocking[entry.second][j];
	    is_first = false;
	  } else {
	    assert(nonblocking == case_output_nonblocking[entry.second][j]);
	  }
	} else {
          case_output_ids[entry.second][j] = current_id;
        }
      }
      assert(!is_first);
      ExprId new_id = expr_builder.create_case(case_id, case_values, std::move(case_output_ids[entry.second]));
      if (!nonblocking) {
	current_values()[entry.first] = new_id;
      }
      output_names().push_back(entry.first);
      output_nonblocking().push_back(nonblocking);
      output_ids().push_back(new_id);
    }
    context_stack_.erase(context_stack_.begin() + stack_index, context_stack_.end());
  }

  ExprId StmtBuilder::get_clock() {
    ExprBuilder expr_builder = make_expr_builder();
    if (timing_events_.size() != 1) {
      // TODO: handle async reset
      throw std::logic_error("FF requires exactly one timing event for now");
    }
    const auto& ev = timing_events_[0];
    if (ev.edge == EdgeKind::kNone) {
      throw std::logic_error("FF timing must be edge-triggered");
    }
    // TODO: handle iff (enable) too
    ExprId clk_id = ev.expr_id;
    switch (ev.edge) {
    case EdgeKind::kPosedge:
      return clk_id;
    case EdgeKind::kNegedge:
      return expr_builder.create_logical_not(clk_id);
    case EdgeKind::kBothEdges:
      return expr_builder.create_both_edge(clk_id);
    case EdgeKind::kNone:
    default:
      throw std::logic_error("Invalid FF edge kind");
    }
  }

  ExprId StmtBuilder::compute_missing_path(ExprId expr_id, ExprBuilder &expr_builder) const {
    if (expr_id == kInvalidExprId) {
      return expr_builder.get_constant_one();
    }
    // miss is kInvalidExprId (treated as const 0) for assigned branches/cases
    const auto &node = expr_builder.get_node(expr_id);
    switch (node.op) {
    case ExprGraph::Op::kMux: {
      const ExprId cond_id = node.operands[0];
      auto recurse = [&](const ExprId data_id, const bool is_complemented) -> ExprId {
        const ExprId child_id = compute_missing_path(data_id, expr_builder);
        if (child_id == kInvalidExprId) {
          return kInvalidExprId;
        }
        ExprId new_cond_id = cond_id;
        if (is_complemented) {
          new_cond_id = expr_builder.create_logical_not(cond_id);
        }
        return expr_builder.create_and({child_id, new_cond_id});
      };
      const ExprId miss_t_id = recurse(node.operands[1], false);
      const ExprId miss_f_id = recurse(node.operands[2], true);
      if (miss_t_id == kInvalidExprId) {
        return miss_f_id;
      }
      if (miss_f_id == kInvalidExprId) {
        return miss_t_id;
      }
      const ExprId miss_id = expr_builder.create_or({miss_t_id, miss_f_id});
      return miss_id;
    }
    case ExprGraph::Op::kCase: {
      const ExprId selector_id = node.operands[0];
      size_t i = 1;
      std::vector<ExprId> cond_ids, miss_ids;
      // operands = [selector, value0, data0, value1, data1, ..., default?]
      while (i + 1 < node.operands.size()) {
        const ExprId case_value = node.operands[i++];
        const ExprId data_id = node.operands[i++];
        const ExprId child_id = compute_missing_path(data_id, expr_builder);
        if (child_id == kInvalidExprId) {
          cond_ids.push_back(kInvalidExprId);
        } else {
          const ExprId cond_id = expr_builder.create_match(selector_id, case_value);
          cond_ids.push_back(cond_id);
          const ExprId miss_id = expr_builder.create_and({child_id, cond_id});
          miss_ids.push_back(miss_id);
        }
      }
      // default if odd count
      if (i < node.operands.size()) {
        const ExprId data_id = node.operands[i];
        const ExprId child_id = compute_missing_path(data_id, expr_builder);
        if (child_id != kInvalidExprId) {
          if (cond_ids.empty()) {
            miss_ids.push_back(child_id);
          } else {
            for (size_t j = 0; j < cond_ids.size(); j++) {
              if (cond_ids[j] == kInvalidExprId) {
                const ExprId case_value = node.operands[2 * j + 1];
                cond_ids[j] = expr_builder.create_match(selector_id, case_value);
              }
            }
            ExprId cond_id = expr_builder.create_or(std::move(cond_ids));
            cond_id = expr_builder.create_logical_not(cond_id);
            const ExprId miss_id = expr_builder.create_and({child_id, cond_id});
            miss_ids.push_back(miss_id);
          }
        }
      }
      if (miss_ids.empty()) {
        return kInvalidExprId;
      }
      return expr_builder.create_or(std::move(miss_ids));
    }
    default: {
      std::vector<ExprId>  miss_ids;
      for (auto data_id : node.operands) {
        ExprId miss_id = compute_missing_path(data_id, expr_builder);
        if (miss_id != kInvalidExprId) {
          miss_ids.push_back(miss_id);
        }
      }
      if (miss_ids.empty()) {
        return kInvalidExprId;
      }
      return expr_builder.create_or(std::move(miss_ids));
    }
    }
  }
  
} // namespace abys::ir
