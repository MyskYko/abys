#include <iostream>
#include <stdexcept>
#include <cassert>

#include "abys/ir/stmt_builder.h"

namespace abys::ir {

  StmtBuilder::StmtBuilder(ExprGraph &expr_graph): expr_graph_(expr_graph) {
    contexts_.emplace_back(Context({ExprBuilder(expr_graph), {}, {}, {}, {}}));
  }

  ExprBuilder &StmtBuilder::get_expr_builder() {
    return contexts_.back().expr_builder;
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
  void StmtBuilder::add_local_variable(std::string name) {
    contexts_.back().local_names.insert(std::move(name));
  }
  size_t StmtBuilder::get_context_stack_index() const {
    return context_stack_.size();
  }

  const ExprBuilder &StmtBuilder::get_const_expr_builder() const {
    return contexts_.back().expr_builder;
  }
  const std::vector<std::string> &StmtBuilder::const_output_names() const {
    return contexts_.back().output_names;
  }
  const std::vector<bool> &StmtBuilder::const_output_nonblocking() const {
    return contexts_.back().output_nonblocking;
  }
  const std::vector<ExprId> &StmtBuilder::const_output_ids() const {
    return contexts_.back().output_ids;
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
  bool StmtBuilder::is_comb_or_latch() const {
    return policy_ == Policy::CombOrLatch;
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
      if (!is_comb_or_latch() && !is_undecided()) {
        std::cerr << "warning: ignoring level-sensitive event in edge-sensitive procedural block" << std::endl;
        return;
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
    contexts_.emplace_back(Context({ExprBuilder(get_expr_builder()), {}, {}, {}, {}}));
  }

  void StmtBuilder::stack_context() {
    assert(contexts_.size() > 1);
    context_stack_.push_back(std::move(contexts_.back()));
    contexts_.pop_back();    
  }

  void StmtBuilder::transfer_output(const Context &from, size_t i, ExprId expr_id) {
    if (!from.output_nonblocking[i]) {
      get_expr_builder().update_value(from.output_names[i], expr_id);
    }
    output_names().push_back(from.output_names[i]);
    output_nonblocking().push_back(from.output_nonblocking[i]);
    output_ids().push_back(expr_id);
  }

  std::vector<size_t> StmtBuilder::collect_last_output_indices(Context &ctx) {
    std::unordered_map<std::string, size_t> last_index;
    for (size_t i = 0; i < ctx.output_names.size(); i++) {
      const std::string &name = ctx.output_names[i];
      auto it = last_index.find(name);
      if (it != last_index.end()) {
        if (ctx.output_nonblocking[it->second] != ctx.output_nonblocking[i]) {
          if (!is_ff()) {
            throw std::logic_error("Mixed blocking and nonblocking assignments to " + name);
          }
          std::cerr << "warning: treating mixed blocking/nonblocking assignments as nonblocking for " << name << std::endl;
          ctx.output_nonblocking[it->second] = true;
          ctx.output_nonblocking[i] = true;
        }
        it->second = i;
      } else {
        last_index[name] = i;
      }
    }
    std::vector<size_t> indices;
    indices.reserve(last_index.size());
    for (size_t i = 0; i < ctx.output_names.size(); i++) {
      if (last_index[ctx.output_names[i]] == i) {
        indices.push_back(i);
      }
    }
    return indices;
  }
  
  void StmtBuilder::merge_context() {
    assert(contexts_.size() > 1);
    Context child = std::move(contexts_.back());
    contexts_.pop_back();
    for (size_t i : collect_last_output_indices(child)) {
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
    const auto then_indices = collect_last_output_indices(then_ctx);
    const auto else_indices = collect_last_output_indices(else_ctx);
    // compute shared outputs
    std::unordered_set<std::string> then_outputs;
    for (size_t i : then_indices) {
      then_outputs.insert(then_ctx.output_names[i]);
    }
    std::unordered_map<std::string, size_t> shared_output_to_else_index;
    for (size_t i : else_indices) {
      if (then_outputs.count(else_ctx.output_names[i])) {
	shared_output_to_else_index[else_ctx.output_names[i]] = i;
      }
    }
    // append then/shared outputs if not local & update their current values
    ExprBuilder &expr_builder = get_expr_builder();
    for (size_t i : then_indices) {
      assert (then_ctx.local_names.empty()); // assuming locals can only be declared in block, which removes local on merge
      const std::string &name = then_ctx.output_names[i];
      auto it = shared_output_to_else_index.find(name);
      ExprId new_id = kInvalidExprId;
      if (it != shared_output_to_else_index.end()) {
	// shared
	ExprId then_id = then_ctx.output_ids[i];
	ExprId else_id = else_ctx.output_ids[it->second];
	new_id = expr_builder.create_mux(cond_id, then_id, else_id);
	if (then_ctx.output_nonblocking[i] != else_ctx.output_nonblocking[it->second]) {
	  if (!is_ff()) {
	    throw std::logic_error("Mixed blocking/nonblocking assignments to " + name);
	  }
	  std::cerr << "warning: treating mixed blocking/nonblocking assignments as nonblocking for " << name << std::endl;
	  then_ctx.output_nonblocking[i] = true;
	  else_ctx.output_nonblocking[it->second] = true;
	}
      } else {
	// not shared
	ExprId then_id = then_ctx.output_ids[i];
	ExprId else_id = expr_builder.get_current_value(name);
	new_id = expr_builder.create_mux(cond_id, then_id, else_id);
      }
      assert(new_id != kInvalidExprId);
      transfer_output(then_ctx, i, new_id);
    }
    // append else/non-shared outputs if not local & update their current values
    for (size_t i : else_indices) {
      assert(else_ctx.local_names.empty());
      const std::string &name = else_ctx.output_names[i];
      if (shared_output_to_else_index.count(name)) {
	continue;
      }
      // not shared
      ExprId then_id = expr_builder.get_current_value(name);
      ExprId else_id = else_ctx.output_ids[i];
      ExprId new_id = expr_builder.create_mux(cond_id, then_id, else_id);
      transfer_output(else_ctx, i, new_id);
    }
  }

  void StmtBuilder::merge_case(ExprId selector_id, const std::vector<ExprId> &case_values, size_t stack_index) {
    size_t output_count = 0;
    std::unordered_map<std::string, size_t> output_map;
    std::vector<std::vector<ExprId>> case_output_ids;
    std::vector<std::vector<bool>> case_output_nonblocking;
    for (size_t j = 0; j < context_stack_.size() - stack_index; j++) {
      const Context &ctx = context_stack_[j + stack_index];
      for (size_t i = 0; i < ctx.output_names.size(); i++) {
	const std::string &name = ctx.output_names[i];
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
    ExprBuilder &expr_builder = get_expr_builder();
    for (const auto &entry : output_map) {
      ExprId current_id = expr_builder.get_current_value(entry.first);
      bool is_first = true;
      bool nonblocking;
      case_output_ids[entry.second].resize(context_stack_.size() - stack_index, kInvalidExprId);
      case_output_nonblocking[entry.second].resize(context_stack_.size() - stack_index, false);
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
      ExprId new_id = expr_builder.create_case(selector_id, case_values, std::move(case_output_ids[entry.second]));
      if (!nonblocking) {
        expr_builder.update_value(entry.first, new_id);
      }
      output_names().push_back(entry.first);
      output_nonblocking().push_back(nonblocking);
      output_ids().push_back(new_id);
    }
    while (context_stack_.size() > stack_index) {
      context_stack_.pop_back();
    }
  }

  void StmtBuilder::get_timing_spec(const std::vector<std::pair<std::string, ExprId>> &outputs, std::string &clk_name, SignalWidth &clk_width, bool &clk_sign, EdgeKind &clk_edge, std::string &rst_name, SignalWidth &rst_width, bool &rst_sign, EdgeKind &rst_edge) const {
    if (timing_events_.size() != 1 && timing_events_.size() != 2) {
      throw std::logic_error("FF requires one or two timing events");
    }
    const ExprBuilder &expr_builder = contexts_.back().expr_builder;
    auto get_input_spec = [&](int index, ExprId &input_id, std::string &name, SignalWidth &width, bool &sign, EdgeKind &edge) {
      const auto &ev = timing_events_[index];
      if (ev.edge == EdgeKind::kNone) {
        throw std::logic_error("FF timing must be edge-triggered");
      }
      edge = ev.edge;
      expr_builder.get_input_spec(ev.expr_id, input_id, name, width, sign);
    };
    if (timing_events_.size() == 1) {
      ExprId input_id;
      get_input_spec(0, input_id, clk_name, clk_width, clk_sign, clk_edge);
      return;
    } else {
      for (int i = 0; i < 2; i++) {
        // TODO: check if rst is used only as an if-cond for debugging
        ExprId input_id;
        std::string name;
        SignalWidth width;
        bool sign;
        EdgeKind edge;
        get_input_spec(i, input_id, name, width, sign, edge);
        bool fUsed = false;
        for (const auto &kv : outputs) {
          if (expr_builder.check_dependency(kv.second, input_id)) {
            fUsed = true;
            break;
          }
        }
        if (fUsed) {
          if (!rst_name.empty()) {
            throw std::logic_error("Ambiguous reset inference");
          }
          rst_name = name;
          rst_width = width;
          rst_sign = sign;
          rst_edge = edge;
        } else {
          if (!clk_name.empty()) {
            throw std::logic_error("Ambiguous clock inference");
          }
          clk_name = name;
          clk_width = width;
          clk_sign = sign;
          clk_edge = edge;
        }
      }
    }
    // TODO: handle iff (enable) too
  }

} // namespace abys::ir
