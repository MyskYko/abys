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
    void add_signal(ModuleId module_id, std::string name, Signal signal);
    void add_input_spec(ModuleId module_id, NodeId node_id, SignalSpec input_spec);

  public:
    explicit TigBuilder(Tig &design);
    void set_top_module(std::string name);
    std::string generate_temporary_name();
    
    ModuleId create_module(std::string name);

    NodeId create_module_input(ModuleId module_id, std::string name, SignalWidth width, bool sign);
    NodeId create_module_output(ModuleId module_id, std::string name, SignalWidth width, bool sign,
                                std::string input_name, SignalWidth input_width, bool input_sign,
                                NodeId input_id = kInvalidNodeId, PortIndex port_idx = 0);

    void create_variable(ModuleId module_id, std::string name, SignalWidth width, bool sign, bool wire, bool reg);
    void create_packed_variable(ModuleId module_id, std::string name, std::vector<SignalWidth> dims, SignalWidth width, bool sign, bool wire, bool reg);
    
    NodeId create_instance(ModuleId module_id, std::string name, ModuleId instance_module_id);
    NodeId create_operation(ModuleId module_id);
    NodeId create_merge(ModuleId module_id);
    void record_ff(ModuleId module_id, std::string name, SignalSpec clk_spec, NodeId node_id, PortIndex port_idx);

    void add_node_input(ModuleId module_id, NodeId node_id, NodeId input_id, PortIndex port_idx = 0);
    void add_node_input_spec(ModuleId module_id, NodeId node_id, std::string name, SignalWidth width, bool sign);
    void finalize_node_input(ModuleId module_id, NodeId node_id);
    PortIndex add_node_output(ModuleId module_id, NodeId node_id, std::string name, SignalWidth width, bool sign, ExprId expr_id = kInvalidExprId, bool comb = false);
    PortIndex add_node_output_expr(ModuleId module_id, NodeId node_id, std::string name, ExprId expr_id, bool comb);

    ExprGraph &get_expr_graph(ModuleId module_id, NodeId node_id);
    void insert_ffs(ModuleId module_id);
    void wire_connections(ModuleId module_id);

    void add_subroutine(Tig::Subroutine subr);
    void flatten_calls();

    void set_node_input(ModuleId module_id, NodeId node_id, PortIndex port_idx, Signal input);

    Signal get_node_input(ModuleId module_id, NodeId node_id, PortIndex port_idx);

    SignalSpec get_signal_spec(ModuleId module_id, Signal signal);
  };

} // namespace abys::ir
