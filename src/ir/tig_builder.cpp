#include "abys/ir/tig_builder.h"

#include <cassert>
#include <memory>
#include <string_view>
#include <unordered_map>

namespace abys::ir {

TigBuilder::NodeId TigBuilder::create_node(Module &module, NodeKind kind) {
  NodeId node_id = static_cast<NodeId>(module.nodes.size());
  module.nodes.emplace_back();
  module.nodes.back().kind = kind;
  return node_id;
}

void TigBuilder::add_signal(Module &module, std::string_view name, EdgeRef edge) {
  auto [it, inserted] = module.signal_map.emplace(std::string(name), edge);
  assert(inserted);
  (void)it;
}

TigBuilder::ModuleId TigBuilder::create_module(std::string name) {
  ModuleId module_id = static_cast<ModuleId>(design_.modules.size());
  design_.modules.emplace_back();
  design_.modules.back().name = std::move(name);
  return module_id;
}

TigBuilder::NodeId TigBuilder::create_module_input(ModuleId module_id, std::string name,
                                                   SignalWidth width, bool sign) {
  Module &module = design_.modules[module_id];
  module.input_ports.emplace_back(name, width, sign);
  NodeId node_id = create_node(module, NodeKind::kPi);
  auto &node = module.nodes[node_id];
  node.outputs.emplace_back(name, width, sign);
  add_signal(module, name, {node_id, 0});
  return node_id;
}

TigBuilder::NodeId TigBuilder::create_module_output(ModuleId module_id, std::string name,
                                                    SignalWidth width, bool sign, NodeId input_id,
                                                    PortIndex port_idx) {
  Module &module = design_.modules[module_id];
  module.output_ports.emplace_back(Module::Port{name, width, sign});
  NodeId node_id = create_node(module, NodeKind::kPo);
  auto &node = module.nodes[node_id];
  node.inputs.emplace_back(input_id, port_idx);
  return node_id;
}

TigBuilder::NodeId TigBuilder::create_conversion_node(ModuleId module_id, std::string name,
                                                      SignalWidth width, bool sign,
                                                      NodeId input_id, PortIndex port_idx) {
  Module &module = design_.modules[module_id];
  NodeId node_id = create_node(module, NodeKind::kConvert);
  auto &node = module.nodes[node_id];
  node.inputs.emplace_back(input_id, port_idx);
  node.outputs.emplace_back(name, width, sign);
  add_signal(module, name, {node_id, 0});
  return node_id;
}

TigBuilder::NodeId TigBuilder::create_instance(ModuleId module_id, std::string name,
                                               ModuleId instance_module_id,
                                               std::vector<Signal> &node_inputs,
                                               std::vector<SignalSpec> &node_outputs) {
  Module &module = design_.modules[module_id];
  NodeId node_id = create_node(module, NodeKind::kInstance);
  auto &node = module.nodes[node_id];
  node.name = std::move(name);
  node.module_id = instance_module_id;
  node.inputs = node_inputs;
  node.outputs = node_outputs;
  for (size_t i = 0; i < node.outputs.size(); i++) {
    const PortIndex port_idx = static_cast<PortIndex>(i);
    add_signal(module, node.outputs[i].name, {node_id, port_idx});
  }
  return node_id;
}

void TigBuilder::set_node_input(ModuleId module_id, NodeId node_id, PortIndex port_idx,
                                Signal input) {
  Module &module = design_.modules[module_id];
  auto &node = module.nodes[node_id];
  node.inputs[port_idx] = input;
}

TigBuilder::Signal TigBuilder::get_node_input(ModuleId module_id, NodeId node_id,
                                              PortIndex port_idx) {
  Module &module = design_.modules[module_id];
  const auto &node = module.nodes[node_id];
  return node.inputs[port_idx];
}

TigBuilder::SignalSpec TigBuilder::get_signal_spec(ModuleId module_id, Signal signal) {
  Module &module = design_.modules[module_id];
  const auto &node = module.nodes[signal.node_id];
  assert(signal.port_idx < node.outputs.size());
  return node.outputs[signal.port_idx];
}

TigBuilder::Signal TigBuilder::find_signal(ModuleId module_id, std::string name) {
  Module &module = design_.modules[module_id];
  auto it = module.signal_map.find(name);
  assert(it != module.signal_map.end());
  return it->second;
}

} // namespace abys::ir
