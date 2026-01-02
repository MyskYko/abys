#include "abys/ir/tig_builder.h"

#include <cassert>
#include <string_view>
#include <unordered_map>

#include "abys/ir/expr_builder.h"
namespace abys::ir {

TigBuilder::NodeId TigBuilder::create_node(ModuleId module_id, NodeKind kind) {
  Module &module = design_.modules[module_id];
  NodeId node_id = static_cast<NodeId>(module.nodes.size());
  module.nodes.emplace_back();
  module.nodes.back().kind = kind;
  return node_id;
}

void TigBuilder::add_signal(ModuleId module_id, std::string_view name, Signal signal) {
  Module &module = design_.modules[module_id];
  auto [it, inserted] = module.signal_map.emplace(std::string(name), std::move(signal));
  assert(inserted);
  (void)it;
}

void TigBuilder::add_input_spec(ModuleId module_id, NodeId node_id, SignalSpec input_spec) {
  if(module_id >= static_cast<ModuleId>(input_specs.size())) {
    input_specs.resize(module_id + 1);
  }
  if(node_id >= static_cast<NodeId>(input_specs[module_id].size())) {
    input_specs[module_id].resize(node_id + 1);
  }
  input_specs[module_id][node_id].push_back(std::move(input_spec));
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
  NodeId node_id = create_node(module_id, NodeKind::kPi);
  Node &node = module.nodes[node_id];
  node.outputs.emplace_back(name, width, sign);
  add_signal(module_id, name, {node_id, 0});
  return node_id;
}

TigBuilder::NodeId TigBuilder::create_module_output(ModuleId module_id, std::string name,
                                                    SignalWidth width, bool sign,
						    std::string input_name,
						    SignalWidth input_width,
						    bool input_sign, NodeId input_id,
						    PortIndex port_idx) {
  Module &module = design_.modules[module_id];
  module.output_ports.emplace_back(name, width, sign);
  NodeId node_id = create_node(module_id, NodeKind::kPo);
  Node &node = module.nodes[node_id];
  if(!input_name.empty()) {
    add_input_spec(module_id, node_id, {std::move(input_name), input_width, input_sign});
  }
  node.inputs.emplace_back(input_id, port_idx);
  return node_id;
}

TigBuilder::NodeId TigBuilder::create_instance(ModuleId module_id, std::string name,
                                               ModuleId instance_module_id) {
  Module &module = design_.modules[module_id];
  NodeId node_id = create_node(module_id, NodeKind::kInstance);
  Node &node = module.nodes[node_id];
  node.name = std::move(name);
  node.module_id = instance_module_id;
  return node_id;
}

TigBuilder::NodeId TigBuilder::create_operation(ModuleId module_id) {
  NodeId node_id = create_node(module_id, NodeKind::kOp);
  return node_id;
}

void TigBuilder::add_node_input(ModuleId module_id, NodeId node_id, NodeId input_id, PortIndex port_idx) {
  Module &module = design_.modules[module_id];
  Node &node = module.nodes[node_id];
  node.inputs.emplace_back(input_id, port_idx);
  add_input_spec(module_id, node_id, {"", 0, false});
}
  
void TigBuilder::add_node_input_spec(ModuleId module_id, NodeId node_id, std::string name, SignalWidth width, bool sign) {
  Module &module = design_.modules[module_id];
  Node &node = module.nodes[node_id];
  node.inputs.emplace_back();
  add_input_spec(module_id, node_id, {std::move(name), width, sign});
}
 
void TigBuilder::finalize_node_input(ModuleId module_id, NodeId node_id) {
  if (module_id >= static_cast<ModuleId>(input_specs.size())) {
    return;
  }
  if (node_id >= static_cast<NodeId>(input_specs[module_id].size())) {
    return;
  }
  bool fUseSpec = false;
  for (SignalSpec const &input_spec : input_specs[module_id][node_id]) {
    if (!input_spec.name.empty()) {
      fUseSpec = true;
      break;
    }
  }
  if (!fUseSpec) {
    input_specs[module_id][node_id].clear();
  }
}
 
void TigBuilder::add_node_output(ModuleId module_id, NodeId node_id, std::string name, SignalWidth width, bool sign) {
  Module &module = design_.modules[module_id];
  Node &node = module.nodes[node_id];
  const PortIndex port_idx = static_cast<PortIndex>(node.outputs.size());
  node.outputs.emplace_back(std::move(name), width, sign);
  add_signal(module_id, node.outputs[port_idx].name, {node_id, port_idx});
}

std::vector<ExprNode> &TigBuilder::get_expr_nodes_ref(ModuleId module_id, NodeId node_id) {
  Module &module = design_.modules[module_id];
  Node &node = module.nodes[node_id];
  return node.expr_nodes;
}

void TigBuilder::wire_connections(ModuleId module_id) {
  for (size_t node_id = 0; node_id < input_specs[module_id].size(); node_id++) {
    auto &specs = input_specs[module_id][node_id];
    for (size_t i = 0; i < specs.size(); i++) {
      const std::string &name = specs[i].name;
      if (!name.empty()) {
        Signal input = find_signal(module_id, name);
        assert(input.node_id != kInvalidNodeId);
        const auto spec = get_signal_spec(module_id, input);
        assert(specs[i].width == spec.width);
        assert(specs[i].sign == spec.sign);
        set_node_input(module_id, static_cast<NodeId>(node_id), static_cast<PortIndex>(i),
                       input);
      } else {
        Signal input =
            get_node_input(module_id, static_cast<NodeId>(node_id), static_cast<PortIndex>(i));
        assert(input.node_id != kInvalidNodeId);
      }
    }
  }
}

void TigBuilder::set_node_input(ModuleId module_id, NodeId node_id, PortIndex port_idx,
                                Signal input) {
  Module &module = design_.modules[module_id];
  Node &node = module.nodes[node_id];
  node.inputs[port_idx] = input;
}

TigBuilder::Signal TigBuilder::get_node_input(ModuleId module_id, NodeId node_id,
                                              PortIndex port_idx) {
  Module &module = design_.modules[module_id];
  const Node &node = module.nodes[node_id];
  return node.inputs[port_idx];
}

TigBuilder::SignalSpec TigBuilder::get_signal_spec(ModuleId module_id, Signal signal) {
  Module &module = design_.modules[module_id];
  const Node &node = module.nodes[signal.node_id];
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
