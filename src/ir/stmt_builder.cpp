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

  void StmtBuilder::create_context() {
    contexts_.emplace_back();
  }

  void StmtBuilder::merge_context() {
    assert(contexts_.size() > 1);
    Context child = std::move(contexts_.back());
    contexts_.pop_back();
    ExprBuilder expr_builder(expr_nodes(), inputs(), current_values());
    // map expr id in child to expr id in parent
    std::unordered_map<ExprId, ExprId> m;
    // check inputs' current values & map if found, create and register otherwise
    for (const auto& input : child.inputs) {
      m[input.id] = expr_builder.find_or_create_input(input.name, child.expr_nodes[input.id].width, child.expr_nodes[input.id].sign);
    }
    // traverse and append non-input expr nodes
    for (ExprId id = 0; id < static_cast<ExprId>(child.expr_nodes.size()); id++) {
      const auto &node = child.expr_nodes[id];
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
    // append outputs if not local & update their current values
    for (size_t i = 0; i < child.output_names.size(); i++) {
      if (child.local_names.count(child.output_names[i])) {
	continue;
      }
      ExprId new_id = m.at(child.output_ids[i]);
      if (!(child.output_nonblocking[i])) {
	current_values()[child.output_names[i]] = new_id;
      }
      output_names().push_back(child.output_names[i]);
      output_nonblocking().push_back(child.output_nonblocking[i]);
      output_ids().push_back(new_id);
    }
  }
  

} // namespace abys::ir
