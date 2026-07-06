#pragma once

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "abys/ir/expr.h"
#include "abys/ir/type.h"

// TODO: unordered_map should always be reserved somehow

namespace abys::ir {

struct Tig {

  using NodeId = uint32_t;
  using ModuleId = uint32_t;
  static constexpr NodeId kInvalidNodeId = std::numeric_limits<NodeId>::max();
  static constexpr ModuleId kInvalidModuleId = std::numeric_limits<ModuleId>::max();

  struct Module {

    struct Port {
      std::string name;
      SignalWidth width = 0;
      bool sign = false;
    };

    struct EdgeRef {
      NodeId node_id = kInvalidNodeId;
      PortIndex port_idx = 0;
    };

    enum class NodeKind {
      kInstance,
      kPi,
      kPo,
      kOp,
      kMerge,
      kFfMerge,
      kFf,
      kLatch,
      kMemory,
      kMacro,
      kUnknown,
    };

    struct Node {
      NodeKind kind = NodeKind::kUnknown;
      std::string name; // instance name
      ModuleId module_id = kInvalidModuleId;
      EdgeKind clk_edge = EdgeKind::kNone; // for ff
      EdgeKind rst_edge = EdgeKind::kNone; // for ff with acync reset
      std::vector<EdgeRef> inputs;
      struct Output {
        std::string name;
        SignalWidth width = 0;
        bool sign = false;
      };
      std::vector<Output> outputs;
      std::vector<ExprId> expr_roots;
      std::vector<bool> combs;
      ExprGraph expr_graph;
    };

    enum class VariableKind { kUnknown, kWire, kReg, kLogic };

    struct Variable {
      VariableKind kind = VariableKind::kUnknown;
      std::string name;
      SignalWidth width = 0;
      bool sign = false;
    };

    struct PackedVariable {
      VariableKind kind = VariableKind::kUnknown;
      std::string name;
      std::vector<SignalWidth> dims;
      SignalWidth width = 0;
      bool sign = false;
    };

    struct PendingFf {
      std::string name;
      Node::Output clk_spec;
      EdgeKind clk_edge;
      Node::Output rst_spec;
      EdgeKind rst_edge;
      NodeId node_id;
      PortIndex port_idx;
    };

    std::string name;
    std::vector<Port> input_ports;
    std::vector<Port> output_ports;
    std::vector<Node> nodes;
    std::vector<Variable> variables;
    std::vector<PackedVariable> packed_variables;
    std::unordered_map<std::string, EdgeRef> signal_map; // TOOD: move to builder?
    std::vector<PendingFf> pending_ffs;                  // TOOD: move to builder?
  };

  struct Subroutine {
    struct Port {
      std::string name;
      SignalWidth width = 0;
      bool sign = false;
    };
    const void *subr_ptr = nullptr;
    std::string name; // for debug
    std::vector<Port> inputs;
    ExprGraph expr_graph;
    ExprId expr_root = kInvalidExprId;
  };

  std::string top_module_name;
  std::vector<Module> modules;
  std::unordered_map<std::string, size_t> module_name_counts;
  std::vector<Subroutine> subroutines;
  uint32_t temporary_name_count = 0;
};

} // namespace abys::ir
