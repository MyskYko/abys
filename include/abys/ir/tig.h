#pragma once

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "abys/ir/expr.h"
#include "abys/ir/type.h"

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

      // TODO: are Ri, Ro necessary?
      enum class NodeKind {
	kInstance,
	kPi,
	kPo,
	kRi,
	kRo,
	kOp,
	kFf,
	kLatch,
        kProhibitedLatch,
	kMemory,
	kMacro,
	kUnknown,
      };
      
      struct Node {
	NodeKind kind = NodeKind::kUnknown;
	std::string name; // instance name
	ModuleId module_id = kInvalidModuleId;
	std::vector<EdgeRef> inputs;
	struct Output {
	  std::string name;
	  SignalWidth width = 0;
	  bool sign = false;
	};
	std::vector<Output> outputs;
	std::vector<ExprId> expr_roots;
	ExprGraph expr_graph;
      };
      
      std::string name;
      std::vector<Port> input_ports;
      std::vector<Port> output_ports;
      std::vector<Node> nodes;
      std::unordered_map<std::string, EdgeRef> signal_map;
    };

    std::vector<Module> modules;
  };

} // namespace abys::ir
