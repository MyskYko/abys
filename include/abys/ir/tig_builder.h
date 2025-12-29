#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "abys/ir/tig.h"

namespace abys::ir {

struct TigBuildResult {
  bool ok = false;
  std::string message;
  Tig design;
};

class TigBuilder {
private:
  Tig &design_;

public:
  using NodeId = Tig::NodeId;
  using PortIndex = Tig::PortIndex;
  using ModuleId = Tig::ModuleId;
  using SignalWidth = Tig::SignalWidth;
  static constexpr NodeId kInvalidNodeId = Tig::kInvalidNodeId;
  static constexpr ModuleId kInvalidModuleId = Tig::kInvalidModuleId;
  using Module = Tig::Module;
  using NodeKind = Tig::Module::NodeKind;
  using EdgeRef = Tig::Module::EdgeRef;
  using Signal = EdgeRef;
  using SignalSpec = Tig::Module::Node::Output;

private:
  NodeId create_node(Module &module, NodeKind kind);
  void add_signal(Module &module, std::string_view name, EdgeRef edge);

public:
  explicit TigBuilder(Tig &design) : design_(design) {}

  ModuleId create_module(std::string name);

  NodeId create_module_input(ModuleId module_id, std::string name, SignalWidth width, bool sign);
  NodeId create_module_output(ModuleId module_id, std::string name, SignalWidth width, bool sign,
                              NodeId input_id, PortIndex port_idx = 0);

  NodeId create_conversion_node(ModuleId module_id, std::string name, SignalWidth width, bool sign,
                                NodeId input_id, PortIndex port_idx = 0);

  NodeId create_instance(ModuleId module_id, std::string name, ModuleId instance_module_id,
                         std::vector<Signal> &node_inputs,
                         std::vector<SignalSpec> &node_outputs);

  void set_node_input(ModuleId module_id, NodeId node_id, PortIndex port_idx, Signal input);

  Signal get_node_input(ModuleId module_id, NodeId node_id, PortIndex port_idx);

  SignalSpec get_signal_spec(ModuleId module_id, Signal signal);

  Signal find_signal(ModuleId module_id, std::string name);
};

} // namespace abys::ir
