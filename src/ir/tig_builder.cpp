#include "abys/ir/tig_builder.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

#include "abys/ir/expr_builder.h"
#include "abys/ir/tig_builder.h"

namespace abys::ir {

TigBuilder::TigBuilder(Tig &design) : design_(design) {}

void TigBuilder::set_top_module(std::string name) {
  design_.top_module_name = name;
}

std::string TigBuilder::generate_temporary_name() {
  return std::string("abys_temporary_") + std::to_string(design_.temporary_name_count++);
}

std::string TigBuilder::create_temporary_signal(ModuleId module_id, SignalWidth width, bool sign) {
  std::string name = generate_temporary_name();
  create_variable(module_id, name, width, sign, true, false);
  return name;
}

TigBuilder::NodeId TigBuilder::create_node(ModuleId module_id, NodeKind kind) {
  Module &module = design_.modules[module_id];
  NodeId node_id = static_cast<NodeId>(module.nodes.size());
  module.nodes.emplace_back();
  module.nodes.back().kind = kind;
  return node_id;
}

void TigBuilder::add_signal(ModuleId module_id, std::string name, Signal signal) {
  Module &module = design_.modules[module_id];
  auto [it, inserted] = module.signal_map.emplace(std::move(name), std::move(signal));
  assert(inserted);
  (void)it;
}

void TigBuilder::add_input_spec(ModuleId module_id, NodeId node_id, SignalSpec input_spec) {
  if (module_id >= static_cast<ModuleId>(input_specs.size())) {
    input_specs.resize(module_id + 1);
  }
  if (node_id >= static_cast<NodeId>(input_specs[module_id].size())) {
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
  add_signal(module_id, std::move(name), {node_id, 0});
  return node_id;
}

TigBuilder::NodeId TigBuilder::create_module_output(ModuleId module_id, std::string name,
                                                    SignalWidth width, bool sign,
                                                    std::string input_name, SignalWidth input_width,
                                                    bool input_sign, NodeId input_id,
                                                    PortIndex port_idx) {
  Module &module = design_.modules[module_id];
  module.output_ports.emplace_back(name, width, sign);
  NodeId node_id = create_node(module_id, NodeKind::kPo);
  Node &node = module.nodes[node_id];
  if (!input_name.empty()) {
    add_input_spec(module_id, node_id, {std::move(input_name), input_width, input_sign});
  }
  node.inputs.emplace_back(input_id, port_idx);
  return node_id;
}

void TigBuilder::create_variable(ModuleId module_id, std::string name, SignalWidth width, bool sign,
                                 bool wire, bool reg) {
  Module &module = design_.modules[module_id];
  Module::VariableKind kind = Module::VariableKind::kLogic;
  assert(!wire || !reg);
  if (wire) {
    kind = Module::VariableKind::kWire;
  } else if (reg) {
    kind = Module::VariableKind::kReg;
  }
  module.variables.push_back({kind, std::move(name), width, sign});
}

void TigBuilder::create_packed_variable(ModuleId module_id, std::string name,
                                        std::vector<SignalWidth> dims, SignalWidth width, bool sign,
                                        bool wire, bool reg) {
  Module &module = design_.modules[module_id];
  Module::VariableKind kind = Module::VariableKind::kLogic;
  assert(!wire || !reg);
  if (wire) {
    kind = Module::VariableKind::kWire;
  } else if (reg) {
    kind = Module::VariableKind::kReg;
  }
  module.packed_variables.push_back({kind, std::move(name), std::move(dims), width, sign});
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

TigBuilder::NodeId TigBuilder::create_merge(ModuleId module_id) {
  NodeId node_id = create_node(module_id, NodeKind::kMerge);
  return node_id;
}

TigBuilder::NodeId TigBuilder::create_ff_merge(ModuleId module_id) {
  NodeId node_id = create_node(module_id, NodeKind::kFfMerge);
  return node_id;
}

void TigBuilder::record_ff(ModuleId module_id, std::string name, SignalSpec clk_spec,
                           EdgeKind clk_edge, SignalSpec rst_spec, EdgeKind rst_edge,
                           NodeId node_id, PortIndex port_idx) {
  assert(!clk_spec.name.empty());
  design_.modules[module_id].pending_ffs.emplace_back(
      Tig::Module::PendingFf{std::move(name), std::move(clk_spec), clk_edge, std::move(rst_spec),
                             rst_edge, node_id, port_idx});
}

void TigBuilder::add_node_input(ModuleId module_id, NodeId node_id, NodeId input_id,
                                PortIndex port_idx) {
  Module &module = design_.modules[module_id];
  Node &node = module.nodes[node_id];
  node.inputs.emplace_back(input_id, port_idx);
  add_input_spec(module_id, node_id, {"", 0, false});
}

void TigBuilder::add_node_input_spec(ModuleId module_id, NodeId node_id, std::string name,
                                     SignalWidth width, bool sign) {
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

PortIndex TigBuilder::add_node_output(ModuleId module_id, NodeId node_id, std::string name,
                                      SignalWidth width, bool sign, ExprId expr_id, bool comb) {
  Module &module = design_.modules[module_id];
  const PortIndex port_idx = static_cast<PortIndex>(module.nodes[node_id].outputs.size());
  std::string final_name;
  if (!name.empty()) {
    auto it = module.signal_map.find(name);
    if (it != module.signal_map.end()) {
      assert(get_signal_spec(module_id, it->second).width == width);
      assert(get_signal_spec(module_id, it->second).sign == sign);
      assert(it->second.node_id != kInvalidNodeId);
      Node &current_node = module.nodes[it->second.node_id];
      if (current_node.kind != NodeKind::kMerge) {
        if (current_node.kind == NodeKind::kOp) {
          // TODO: handle multi-driver
          current_node.outputs[it->second.port_idx].name.clear();
        }
        NodeId merge_id = create_merge(module_id); // current_node may be invalidated here
        Node &merge_node = module.nodes[merge_id];
        merge_node.outputs.emplace_back(std::move(name), width, sign);
        merge_node.expr_roots.push_back(kInvalidExprId);
        merge_node.combs.push_back(false);
        add_node_input(module_id, merge_id, it->second.node_id, it->second.port_idx);
        it->second = Signal{merge_id, 0};
      }
      add_node_input(module_id, it->second.node_id, node_id, port_idx);
    } else {
      add_signal(module_id, name, {node_id, port_idx});
      final_name = std::move(name);
    }
  }
  Node &node = module.nodes[node_id];
  node.outputs.emplace_back(std::move(final_name), width, sign);
  node.expr_roots.push_back(expr_id);
  node.combs.push_back(comb);
  return port_idx;
}

PortIndex TigBuilder::add_node_output_expr(ModuleId module_id, NodeId node_id, std::string name,
                                           ExprId expr_id, bool comb) {
  const Module &module = design_.modules[module_id];
  const Node &node = module.nodes[node_id];
  const auto &expr_node = node.expr_graph.nodes[expr_id];
  return add_node_output(module_id, node_id, std::move(name), expr_node.width, expr_node.sign,
                         expr_id, comb);
}

ExprGraph &TigBuilder::get_expr_graph(ModuleId module_id, NodeId node_id) {
  Module &module = design_.modules[module_id];
  Node &node = module.nodes[node_id];
  return node.expr_graph;
}

void TigBuilder::insert_ffs(ModuleId module_id) {
  auto same_ff_props = [](const Tig::Module::PendingFf &a, const Tig::Module::PendingFf &b) {
    if (a.clk_spec.name != b.clk_spec.name || a.clk_spec.width != b.clk_spec.width ||
        a.clk_spec.sign != b.clk_spec.sign || a.clk_edge != b.clk_edge) {
      return false;
    }
    const bool a_has_rst = !a.rst_spec.name.empty();
    const bool b_has_rst = !b.rst_spec.name.empty();
    if (a_has_rst != b_has_rst) {
      return false;
    }
    if (!a_has_rst) {
      return true;
    }
    if (a.rst_spec.name != b.rst_spec.name || a.rst_spec.width != b.rst_spec.width ||
        a.rst_spec.sign != b.rst_spec.sign || a.rst_edge != b.rst_edge) {
      return false;
    }
    return true;
  };

  auto &module = design_.modules[module_id];

  auto create_ff = [&](const PendingFf &pending_ff, const Signal &signal, const SignalSpec &spec,
                       bool named) -> Signal {
    NodeId ff_id = create_node(module_id, NodeKind::kFf);
    Node &ff_node = module.nodes[ff_id];
    add_node_input(module_id, ff_id, signal.node_id, signal.port_idx);
    add_node_input_spec(module_id, ff_id, pending_ff.clk_spec.name, pending_ff.clk_spec.width,
                        pending_ff.clk_spec.sign);
    ff_node.clk_edge = pending_ff.clk_edge;
    if (!pending_ff.rst_spec.name.empty()) {
      add_node_input_spec(module_id, ff_id, pending_ff.rst_spec.name, pending_ff.rst_spec.width,
                          pending_ff.rst_spec.sign);
      ff_node.rst_edge = pending_ff.rst_edge;
    }
    ff_node.outputs.emplace_back(named ? pending_ff.name : "", spec.width, spec.sign);
    ff_node.expr_roots.push_back(kInvalidExprId);
    ff_node.combs.push_back(false);
    return Signal{ff_id, 0};
  };

  auto &pending_ffs = module.pending_ffs;
  std::sort(pending_ffs.begin(), pending_ffs.end(),
            [](const PendingFf &a, const PendingFf &b) { return a.name < b.name; });
  for (size_t begin = 0; begin < pending_ffs.size();) {
    auto it = module.signal_map.find(pending_ffs[begin].name);
    if (it == module.signal_map.end()) {
      throw std::logic_error("insert_ffs: signal not found: " + pending_ffs[begin].name);
    }
    const auto spec = get_signal_spec(module_id, it->second);
    assert(module.nodes[it->second.node_id].kind == NodeKind::kOp ||
           module.nodes[it->second.node_id].kind == NodeKind::kMerge);
    module.nodes[it->second.node_id].outputs[it->second.port_idx].name.clear();
    size_t end = begin + 1;
    while (end < pending_ffs.size() && pending_ffs[end].name == pending_ffs[begin].name) {
      ++end;
    }
    if (begin + 1 == end) {
      it->second = create_ff(pending_ffs[begin], it->second, spec, true);
      begin = end;
      continue;
    }
    std::vector<std::vector<size_t>> clusters;
    for (size_t i = begin; i < end; ++i) {
      bool f = false;
      for (auto &cluster : clusters) {
        if (same_ff_props(pending_ffs[i], pending_ffs[cluster.front()])) {
          cluster.push_back(i);
          f = true;
          break;
        }
      }
      if (!f) {
        clusters.push_back({i});
      }
    }
    if (clusters.size() == 1) {
      it->second = create_ff(pending_ffs[clusters.front().front()], it->second, spec, true);
    } else {
      std::vector<Signal> ffs;
      for (const auto &cluster : clusters) {
        Signal signal;
        if (cluster.size() == 1) {
          signal = {pending_ffs[cluster.front()].node_id, pending_ffs[cluster.front()].port_idx};
        } else {
          NodeId merge_id = create_merge(module_id);
          Node &merge_node = module.nodes[merge_id];
          for (size_t i : cluster) {
            const auto &pending_ff = pending_ffs[i];
            add_node_input(module_id, merge_id, pending_ff.node_id, pending_ff.port_idx);
          }
          merge_node.outputs.emplace_back("", spec.width, spec.sign);
          merge_node.expr_roots.push_back(kInvalidExprId);
          merge_node.combs.push_back(false);
          signal = Signal{merge_id, 0};
        }
        ffs.push_back(create_ff(pending_ffs[cluster.front()], signal, spec, false));
      }
      NodeId ff_merge_id = create_ff_merge(module_id);
      Node &ff_merge_node = module.nodes[ff_merge_id];
      for (const auto &ff : ffs) {
        add_node_input(module_id, ff_merge_id, ff.node_id, ff.port_idx);
      }
      ff_merge_node.outputs.emplace_back(pending_ffs[begin].name, spec.width, spec.sign);
      ff_merge_node.expr_roots.push_back(kInvalidExprId);
      ff_merge_node.combs.push_back(false);
      it->second = Signal{ff_merge_id, 0};
    }
    begin = end;
  }
  module.pending_ffs.clear();
}

void TigBuilder::wire_connections(ModuleId module_id) {
  Module &module = design_.modules[module_id];
  for (size_t node_id = 0; node_id < input_specs[module_id].size(); ++node_id) {
    auto &specs = input_specs[module_id][node_id];
    for (size_t i = 0; i < specs.size(); ++i) {
      const std::string &name = specs[i].name;
      if (!name.empty()) {
        const auto it = module.signal_map.find(name);
        if (it == module.signal_map.end()) {
          std::cerr << "warning: leaving unresolved signal input unconnected: " << module.name
                    << "." << name << "\n"; // TODO: decide warning system
          continue;
        }
        assert(it->second.node_id != kInvalidNodeId);
        const auto spec = get_signal_spec(module_id, it->second);
        assert(specs[i].width == spec.width);
        assert(specs[i].sign == spec.sign);
        set_node_input(module_id, static_cast<NodeId>(node_id), static_cast<PortIndex>(i),
                       it->second);
      } else {
        Signal input =
            get_node_input(module_id, static_cast<NodeId>(node_id), static_cast<PortIndex>(i));
        if (input.node_id == kInvalidNodeId) {
          // TODO: unassigned inputs is currently handled here
          continue;
        }
      }
    }
  }
}

void TigBuilder::add_subroutine(Tig::Subroutine subr) {
  design_.subroutines.push_back(std::move(subr));
}

void TigBuilder::flatten_calls() {
  std::unordered_map<const void *, size_t> subr_map;
  subr_map.reserve(design_.subroutines.size());
  for (size_t i = 0; i < design_.subroutines.size(); ++i) {
    subr_map.emplace(design_.subroutines[i].subr_ptr, i);
  }
  for (auto &module : design_.modules) {
    for (auto &node : module.nodes) {
      ExprGraph &expr_graph = node.expr_graph;
      for (size_t i = 0; i < expr_graph.calls.size(); ++i) {
        auto subr_it = subr_map.find(expr_graph.calls[i].subr_ptr);
        if (subr_it == subr_map.end()) {
          throw std::logic_error("Unknown subroutine: " + expr_graph.calls[i].name);
        }
        const Tig::Subroutine &subr = design_.subroutines[subr_it->second];
        ExprId call_id = expr_graph.calls[i].id;
        if (expr_graph.nodes[call_id].operands.size() != subr.inputs.size()) {
          throw std::logic_error("Call arity mismatch: " + expr_graph.calls[i].name);
        }
        std::unordered_map<ExprId, ExprId> id_map;
        id_map.reserve(subr.expr_graph.nodes.size() + 1);
        id_map.emplace(kInvalidExprId, kInvalidExprId);
        for (size_t j = 0; j < subr.inputs.size(); ++j) {
          const auto input_it = subr.expr_graph.inputs.find(subr.inputs[j].name);
          if (input_it == subr.expr_graph.inputs.end()) {
            throw std::logic_error("Subroutine input not found: " + subr.inputs[j].name);
          }
          id_map.emplace(input_it->second, expr_graph.nodes[call_id].operands[j]);
        }
        for (const auto &constant : subr.expr_graph.constants) {
          if (constant.id == subr.expr_graph.constant_zero) {
            id_map.emplace(constant.id, expr_graph.constant_zero);
          } else if (constant.id == subr.expr_graph.constant_one) {
            id_map.emplace(constant.id, expr_graph.constant_one);
          } else {
            const auto &src = subr.expr_graph.nodes[constant.id];
            const ExprId dst_id = static_cast<ExprId>(expr_graph.nodes.size());
            expr_graph.nodes.push_back(src);
            expr_graph.constants.push_back({dst_id, constant.value});
            id_map.emplace(constant.id, dst_id);
          }
        }
        const auto return_input_it = subr.expr_graph.inputs.find(subr.name);
        if (return_input_it != subr.expr_graph.inputs.end()) {
          id_map.emplace(return_input_it->second, kInvalidExprId);
        }
        for (ExprId src_id = 0; src_id < static_cast<ExprId>(subr.expr_graph.nodes.size());
             ++src_id) {
          if (id_map.find(src_id) != id_map.end()) {
            continue;
          }
          const auto &src = subr.expr_graph.nodes[src_id];
          std::vector<ExprId> new_ops;
          new_ops.reserve(src.operands.size());
          for (ExprId sop : src.operands) {
            auto mit = id_map.find(sop);
            if (mit == id_map.end()) {
              throw std::logic_error("Subroutine graph not topologically ordered");
            }
            new_ops.push_back(mit->second);
          }
          const ExprId dst_id = static_cast<ExprId>(expr_graph.nodes.size());
          expr_graph.nodes.push_back(src);
          expr_graph.nodes.back().operands = std::move(new_ops);
          id_map.emplace(src_id, dst_id);
          if (src.op == ExprGraph::Op::kCall) {
            for (const auto &src_call : subr.expr_graph.calls) {
              if (src_call.id == src_id) {
                expr_graph.calls.push_back({dst_id, src_call.subr_ptr, src_call.name});
                break;
              }
            }
          }
        }
        auto rit = id_map.find(subr.expr_root);
        if (rit == id_map.end()) {
          throw std::logic_error("Failed to map subroutine root: " + subr.name);
        }
        const ExprId inlined_root = rit->second;
        auto &call_node = expr_graph.nodes[call_id];
        call_node.op = ExprGraph::Op::kConvert; // temporary buffer op
        call_node.operands.clear();
        call_node.operands.push_back(inlined_root);
      }
      expr_graph.calls.clear();
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

} // namespace abys::ir
