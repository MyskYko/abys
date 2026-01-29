#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <stdexcept>

#include "abys/ir/expr.h"
#include "abys/ir/tig.h"

namespace abys::ir {

  class StmtBuilder {
  public:
    explicit StmtBuilder();
    
    std::vector<ExprNode>& expr_nodes();
    std::vector<ExprInput>& inputs();
    std::unordered_map<std::string, ExprId>& current_values();
    std::vector<std::string>& output_names();
    std::vector<bool>& output_nonblocking();
    std::vector<ExprId>& output_ids();

    // building
    void set_comb();
    void set_latch();
    void set_comb_or_latch();
    void set_ff();
    
    bool is_root_context() const;
    bool is_ff() const;
    bool is_undecided() const;

    void create_context();
    void merge_context();

    // TODO: merge contexts

    // API for exporting info for creating a tig node
    template<typename Func>
    void for_each_input(Func &&func) {
      for (const auto &in : inputs()) {
	const auto &node = expr_nodes()[in.id];
	func(in.name, node.width, node.sign);
      }
    }

    void transfer_expr_nodes(std::vector<ExprNode>& out) {
      auto &nodes = expr_nodes();
      out.insert(out.end(), std::make_move_iterator(nodes.begin()), std::make_move_iterator(nodes.end()));
      nodes.clear();
    }

    template<typename Func>
    void for_each_output(Func &&func) {
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
	func(kv.first, kv.second.expr_id);
      }
    }

  private:
    enum class Policy {
      Comb,
      Latch,
      CombOrLatch,
      Ff,
      Undecided
    };

    Policy policy_ = Policy::Undecided;
    
    struct Context {
      std::vector<ExprNode> expr_nodes;
      std::vector<ExprInput> inputs;
      std::unordered_map<std::string, ExprId> current_values;
      std::vector<std::string> output_names;
      std::vector<bool> output_nonblocking;
      std::vector<ExprId> output_ids;
      std::unordered_set<std::string> local_names;
    };

    std::vector<Context> contexts_;

  };
  
} // namespace abys::ir
