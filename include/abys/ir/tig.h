#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace abys::ir {

  struct Tig {
    
    using NodeId = uint32_t;
    using PortIndex = uint32_t;
    using ModuleId = uint32_t;
    using SignalWidth = uint64_t;
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
	kRi,
	kRo,
	kConst,
	kSplit,
	kMerge,
	kConvert,
	kOp,
	kUnknown,
      };
      
      struct Node {
	NodeKind kind = NodeKind::kUnknown;
	std::string name; // instance name
	ModuleId module_id = kInvalidModuleId;
	std::string op;
	std::string const_value;
	std::vector<EdgeRef> inputs;
	struct Output {
	  std::string name;
	  SignalWidth width = 0;
	  bool sign = false;
	};
	std::vector<Output> outputs;
	std::vector<SignalWidth> segment_widths;
      };
      
      enum class BlockKind {
	kMemory,
	kLatch,
	kFf,
	kMacro,
	kUnknown,
      };
      
      struct Block {
	BlockKind kind = BlockKind::kUnknown;
	std::string name;
	std::string impl_name;
	std::vector<Port> input_ports;
	std::vector<Port> output_ports;
	std::vector<NodeId> inputs;
	std::vector<NodeId> outputs;
	std::unordered_map<std::string, std::string> params;
	std::unordered_map<std::string, std::string> attributes;
      };

      std::string name;
      std::vector<Port> input_ports;
      std::vector<Port> output_ports;
      std::vector<Node> nodes;
      std::vector<Block> blocks;
      std::unordered_map<std::string, EdgeRef> signal_map;
    };

    std::vector<Module> modules;
  };

} // namespace abys::ir
