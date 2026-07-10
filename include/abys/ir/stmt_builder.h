#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "abys/ir/expr_builder.h"
#include "abys/ir/tig.h"

namespace abys::ir {

class StmtBuilder {
public:
  explicit StmtBuilder(ExprGraph &expr_graph);

  ExprBuilder &get_expr_builder();
  std::vector<std::string> &output_names(); // TODO: this and the following three may be replaced by
                                            // add_output(string, bool, ExprId) API
  std::vector<bool> &output_nonblocking();
  std::vector<ExprId> &output_ids();
  std::unordered_map<std::string, ExprId> &scheduled_assignments();
  void add_local_variable(std::string name);
  size_t get_context_stack_index() const;

  const ExprBuilder &get_const_expr_builder() const;
  const std::vector<std::string> &const_output_names() const;
  const std::vector<bool> &const_output_nonblocking() const;
  const std::vector<ExprId> &const_output_ids() const;

  // building
  void set_comb();
  void set_latch();
  void set_comb_or_latch();
  void set_ff();

  bool is_root_context() const;
  bool is_comb() const;
  bool is_comb_or_latch() const;
  bool is_ff() const;
  bool is_undecided() const;

  bool has_timing() const;

  void add_timing(ExprId expr_id, ExprId iff_id, bool posedge, bool negedge);

  void create_context();
  void stack_context();
  void merge_context();
  void merge_conditional(ExprId cond_id);
  void merge_case(ExprId selector_id, const std::vector<ExprId> &case_values, size_t stack_index,
                  bool full_case = false);

  // API for exporting info for creating a tig node
  void get_timing_spec(const std::vector<std::pair<std::string, ExprId>> &outputs,
                       std::string &clk_name, SignalWidth &clk_width, bool &clk_sign,
                       EdgeKind &clk_edge, std::string &rst_name, SignalWidth &rst_width,
                       bool &rst_sign, EdgeKind &rst_edge) const;

  template <typename Func> void for_each_input(Func &&func) const {
    get_const_expr_builder().for_each_input(func);
  }

  template <typename Func> void for_each_output(Func &&func) {
    assert(!is_undecided()); // we don't allow always without any timing
    struct OutputInfo {
      ExprId expr_id = 0;
      bool seen_blocking = false;
      bool seen_nonblocking = false;
    };
    std::unordered_map<std::string, OutputInfo> info;
    info.reserve(const_output_names().size());
    for (size_t i = 0; i < const_output_names().size(); ++i) {
      const auto &name = const_output_names()[i];
      const bool nonblocking = const_output_nonblocking()[i];
      auto &entry = info[name];
      if (entry.seen_nonblocking) {
        ExprGraph::Node &node = get_expr_builder().get_node(const_output_ids()[i]);
        if (node.op == ExprGraph::Op::kMaskedAssign) {
          // TODO: handle two-sided conditional masked updates, e.g. if (c) a[0] <= 1; else a[1] <=
          // 1;.
          node.operands[0] = entry.expr_id;
        }
      }
      entry.expr_id = const_output_ids()[i];
      entry.seen_blocking |= !nonblocking;
      entry.seen_nonblocking |= nonblocking;
    }
    for (const auto &kv : info) {
      if (kv.second.seen_blocking && kv.second.seen_nonblocking) {
        // TODO: we do not really enforce always_ff with blocking yet
        throw std::logic_error("Mixed blocking and nonblocking assignments to " + kv.first);
      }
      func(kv.first, kv.second.expr_id);
    }
  }

private:
  ExprGraph &expr_graph_;

  enum class Policy { Comb, Latch, CombOrLatch, Ff, Undecided };

  Policy policy_ = Policy::Undecided;

  struct Context {
    ExprBuilder expr_builder;
    std::vector<std::string> output_names;
    std::vector<bool> output_nonblocking;
    std::vector<ExprId> output_ids;
    std::unordered_set<std::string> local_names;
    std::unordered_map<std::string, ExprId> scheduled_assignments;
  };

  std::vector<Context> contexts_;
  std::vector<Context> context_stack_;

  ExprId fallback_value(const std::string &name) const;
  void transfer_output(const Context &from, size_t i, ExprId expr_id);
  std::vector<size_t> collect_last_output_indices(Context &ctx);

  struct TimingEvent {
    EdgeKind edge = EdgeKind::kNone;
    ExprId expr_id = kInvalidExprId;
    ExprId iff_id = kInvalidExprId;
  };

  std::vector<TimingEvent> timing_events_;
};

} // namespace abys::ir
