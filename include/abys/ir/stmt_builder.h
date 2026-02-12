#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <stdexcept>

#include "abys/ir/expr_builder.h"
#include "abys/ir/tig.h"

namespace abys::ir {

  class StmtBuilder {
  public:
    explicit StmtBuilder(ExprGraph &expr_graph);
    
    ExprBuilder make_expr_builder(); // TODO: maybe this can be made per context when it's open
    std::unordered_map<std::string, ExprId> &current_values();
    std::vector<std::string> &output_names();
    std::vector<bool> &output_nonblocking();
    std::vector<ExprId> &output_ids();
    size_t get_context_stack_index() const;
    
    // building
    void set_comb();
    void set_latch();
    void set_comb_or_latch();
    void set_ff();
    
    bool is_root_context() const;
    bool is_comb() const;
    bool is_ff() const;
    bool is_undecided() const;
    
    bool has_timing() const;

    void add_timing(ExprId expr_id, ExprId iff_id, bool posedge, bool negedge);

    void create_context();
    void stack_context();
    void merge_context();
    void merge_conditional(ExprId cond_id);
    void merge_case(ExprId case_id, const std::vector<ExprId> &case_values, size_t index);

    // API for exporting info for creating a tig node
    ExprId get_clock();
    
    ExprId compute_missing_path(ExprId expr_id, ExprBuilder &expr_builder) const;
    
    template<typename Func>
    void for_each_input(Func &&func) const {
      for (const auto &in : expr_graph_.inputs) {
	const auto &node = expr_graph_.nodes[in.id];
	func(in.name, node.width, node.sign);
      }
    }

    template<typename Func>
    void for_each_output(Func &&func) {
      assert(!is_undecided()); // we don't allow always without any timing
      bool has_latch = false;
      ExprBuilder expr_builder = make_expr_builder();
      struct OutputInfo {
	ExprId expr_id = 0;
        bool seen_blocking = false;
	bool seen_nonblocking = false;
      };
      std::unordered_map<std::string, OutputInfo> info;
      info.reserve(output_names().size());
      for (size_t i = 0; i < output_names().size(); ++i) {
	const auto &name = output_names()[i];
	const bool nonblocking = output_nonblocking()[i];
	auto &entry = info[name];
	entry.expr_id = output_ids()[i];
	entry.seen_blocking |= !nonblocking;
	entry.seen_nonblocking |= nonblocking;
      }
      for (const auto &kv : info) {
	if (kv.second.seen_blocking && kv.second.seen_nonblocking) {
	  throw std::logic_error("Mixed blocking and nonblocking assignments to " + kv.first);
	}
        ExprId enable_id = kInvalidExprId;
        if (!is_ff()) {
          const ExprId miss_id = compute_missing_path(kv.second.expr_id, expr_builder);
          if (miss_id != kInvalidExprId) {
            has_latch = true;
            enable_id = expr_builder.create_logical_not(miss_id);
          }
        }
	func(kv.first, kv.second.expr_id, enable_id);
      }
      // TODO: warn if always_latch && !has_latch
      (void)has_latch;
    }

  private:
    ExprGraph &expr_graph_;

    enum class Policy {
      Comb,
      Latch,
      CombOrLatch,
      Ff,
      Undecided
    };

    Policy policy_ = Policy::Undecided;
    
    struct Context {
      std::unordered_map<std::string, ExprId> current_values;
      std::vector<std::string> output_names;
      std::vector<bool> output_nonblocking;
      std::vector<ExprId> output_ids;
      std::unordered_set<std::string> local_names;
    };

    std::vector<Context> contexts_;
    std::vector<Context> context_stack_;

    void transfer_output(const Context& from, size_t i, ExprId expr_id);

    enum class EdgeKind {
      kNone,
      kPosedge,
      kNegedge,
      kBothEdges
    };
    
    struct TimingEvent {
      EdgeKind edge = EdgeKind::kNone;
      ExprId expr_id = kInvalidExprId;
      ExprId iff_id = kInvalidExprId;
    };
    
    std::vector<TimingEvent> timing_events_;

  };
  
} // namespace abys::ir
