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
    using ModuleId = Tig::ModuleId;
    static constexpr NodeId kInvalidNodeId = Tig::kInvalidNodeId;
    static constexpr ModuleId kInvalidModuleId = Tig::kInvalidModuleId;
    using Module = Tig::Module;
    using Node = Tig::Module::Node;
    using NodeKind = Tig::Module::NodeKind;
    using EdgeRef = Tig::Module::EdgeRef;
    using Signal = EdgeRef;
    using SignalSpec = Tig::Module::Node::Output;

  private:
    std::vector<std::vector<std::vector<SignalSpec>>> input_specs;

    NodeId create_node(ModuleId module_id, NodeKind kind);
    void add_signal(ModuleId module_id, std::string_view name, Signal signal);
    void add_input_spec(ModuleId module_id, NodeId node_id, SignalSpec input_spec);

  public:
    explicit TigBuilder(Tig &design) : design_(design) {}

    ModuleId create_module(std::string name);

    NodeId create_module_input(ModuleId module_id, std::string name, SignalWidth width, bool sign);
    NodeId create_module_output(ModuleId module_id, std::string name, SignalWidth width, bool sign,
                                std::string input_name, SignalWidth input_width, bool input_sign,
                                NodeId input_id = kInvalidNodeId, PortIndex port_idx = 0);

    NodeId create_instance(ModuleId module_id, std::string name, ModuleId instance_module_id);
    NodeId create_operation(ModuleId module_id);
    NodeId create_ff(ModuleId module_id);

    void add_node_input(ModuleId module_id, NodeId node_id, NodeId input_id, PortIndex port_idx = 0);
    void add_node_input_spec(ModuleId module_id, NodeId node_id, std::string name, SignalWidth width, bool sign);
    void finalize_node_input(ModuleId module_id, NodeId node_id);
    void add_node_output(ModuleId module_id, NodeId node_id, std::string name, SignalWidth width, bool sign);
    void add_node_output_expr(ModuleId module_id, NodeId node_id, std::string name, ExprId expr_id, bool comb);

    ExprGraph &get_expr_graph(ModuleId module_id, NodeId node_id);
    void wire_connections(ModuleId module_id);

    void set_node_input(ModuleId module_id, NodeId node_id, PortIndex port_idx, Signal input);

    Signal get_node_input(ModuleId module_id, NodeId node_id, PortIndex port_idx);

    SignalSpec get_signal_spec(ModuleId module_id, Signal signal);

    Signal find_signal(ModuleId module_id, std::string name);
  };

} // namespace abys::ir
