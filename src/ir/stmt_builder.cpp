#include <cassert>

#include "abys/ir/expr_builder.h"
#include "abys/ir/stmt_builder.h"

namespace abys::ir {

  StmtBuilder::StmtBuilder() {
    contexts_.emplace_back();
  }

  std::vector<ExprNode>& StmtBuilder::expr_nodes() {
    return contexts_.back().expr_nodes;
  }

  std::vector<ExprInput>& StmtBuilder::inputs() {
    return contexts_.back().inputs;
  }

  std::unordered_map<std::string, ExprId>& StmtBuilder::current_values() {
    return contexts_.back().current_values;
  }

  std::vector<std::string>& StmtBuilder::output_names() {
    return contexts_.back().output_names;
  }

  std::vector<bool>& StmtBuilder::output_nonblocking() {
    return contexts_.back().output_nonblocking;
  }

  std::vector<ExprId>& StmtBuilder::output_ids() {
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
    timing_events_.push_back({edge_kind, expr_id, iff_id});
  }
  
  void StmtBuilder::create_context() {
    contexts_.emplace_back();
  }

  void StmtBuilder::stack_context() {
    assert(contexts_.size() > 1);
    context_stack_.push_back(std::move(contexts_.back()));
    contexts_.pop_back();    
  }

  void StmtBuilder::transfer_expr_nodes(const Context& from, std::unordered_map<ExprId, ExprId>& m) {
    ExprBuilder expr_builder(expr_nodes(), inputs(), current_values());
    // check inputs' current values & map if found, create and register otherwise
    for (const auto& input : from.inputs) {
      m[input.id] = expr_builder.find_or_create_input(input.name, from.expr_nodes[input.id].width, from.expr_nodes[input.id].sign);
    }
    // traverse and append non-input expr nodes
    for (ExprId id = 0; id < static_cast<ExprId>(from.expr_nodes.size()); id++) {
      const auto &node = from.expr_nodes[id];
      if (node.op == ExprNode::Op::kInput) {
	continue;
      }
      if (node.op == ExprNode::Op::kConst) {
        m[id] = expr_builder.create_const(node.value, node.width, node.sign);	
	continue;
      }
      std::vector<ExprId> ops;
      ops.reserve(node.operands.size());
      for (auto op : node.operands) {
	ops.push_back(m.at(op));
      }
      m[id] = expr_builder.create_nary(node.op, std::move(ops), node.width, node.sign);
    }
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
    // map expr id in child to expr id in parent
    std::unordered_map<ExprId, ExprId> m;
    transfer_expr_nodes(child, m);
    // append outputs if not local & update their current values
    for (size_t i = 0; i < child.output_names.size(); i++) {
      if (child.local_names.count(child.output_names[i])) {
	continue;
      }
      transfer_output(child, i, m.at(child.output_ids[i]));
    }
  }

  void StmtBuilder::merge_conditional(ExprId cond_id) {
    assert(context_stack_.size() > 1);
    Context else_ctx = std::move(context_stack_.back());
    context_stack_.pop_back();
    Context then_ctx = std::move(context_stack_.back());
    context_stack_.pop_back();
    std::unordered_map<ExprId, ExprId> then_map, else_map;
    transfer_expr_nodes(then_ctx, then_map);
    transfer_expr_nodes(else_ctx, else_map);
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
    ExprBuilder expr_builder(expr_nodes(), inputs(), current_values());
    for (size_t i = 0; i < then_ctx.output_names.size(); i++) {
      assert (then_ctx.local_names.empty());
      const std::string& name = then_ctx.output_names[i];
      auto it = shared_output_to_else_index.find(name);
      ExprId new_id = kInvalidExprId;
      if (it != shared_output_to_else_index.end()) {
	// shared
	ExprId then_id = then_map.at(then_ctx.output_ids[i]);
	ExprId else_id = else_map.at(else_ctx.output_ids[it->second]);
	new_id = expr_builder.create_mux(cond_id, then_id, else_id);
	if (then_ctx.output_nonblocking[i] != else_ctx.output_nonblocking[it->second]) {
	  throw std::logic_error("Mixed blocking/nonblocking assignments to " + name);
	}
      } else {
	// not shared
	ExprId then_id = then_map.at(then_ctx.output_ids[i]);
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
      ExprId else_id = else_map.at(else_ctx.output_ids[i]);
      ExprId new_id = expr_builder.create_mux(cond_id, then_id, else_id);
      transfer_output(else_ctx, i, new_id);
    }
  }


} // namespace abys::ir
