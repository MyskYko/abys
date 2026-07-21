#include <cassert>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "abys/ir/verilog_emitter.h"

namespace abys::ir {

VerilogEmitter::VerilogEmitter(const Tig &design) : design_(design) {}

void VerilogEmitter::emit(std::ostream &os) const {
  bool first = true;
  for (const Module &module : design_.modules) {
    if (!first) {
      os << "\n";
    }
    first = false;
    emit_module(module, os);
  }
}

void VerilogEmitter::emit_module(const Module &module, std::ostream &os) const {
  emit_module_header(module, os);
  emit_signal_decls(module, os);
  emit_instances(module, os);
  emit_combinational(module, os);
  emit_sequential(module, os);
  emit_module_footer(os);
}

void VerilogEmitter::emit_module_header(const Module &module, std::ostream &os) const {
  os << "module " << module.name << " (\n";
  bool first = true;
  for (const auto &input : module.input_ports) {
    if (!first) {
      os << ",\n";
    }
    first = false;
    os << "  input ";
    if (input.sign) {
      os << "signed ";
    }
    if (input.width > 1) {
      os << "[" << (input.width - 1) << ":0] ";
    }
    os << input.name;
  }
  for (const auto &output : module.output_ports) {
    if (!first) {
      os << ",\n";
    }
    first = false;
    os << "  output ";
    // TODO: we are putting everything into always for now
    os << " logic ";
    if (output.sign) {
      os << "signed ";
    }
    if (output.width > 1) {
      os << "[" << (output.width - 1) << ":0] ";
    }
    os << output.name;
  }
  os << ");\n\n";
}

void VerilogEmitter::emit_signal_decls(const Module &module, std::ostream &os) const {
  std::unordered_set<std::string> port_names;
  for (const auto &p : module.input_ports) {
    port_names.insert(p.name);
  }
  for (const auto &p : module.output_ports) {
    port_names.insert(p.name);
  }
  for (const auto &var : module.variables) {
    if (port_names.count(var.name)) {
      continue;
    }
    os << "  ";
    // TODO: we are putting everything into always for now
    // if (var.kind == Module::VariableKind::kWire) {
    //   os << "wire ";
    // }
    os << "logic ";
    if (var.sign) {
      os << "signed ";
    }
    if (var.width > 1) {
      os << "[" << (var.width - 1) << ":0] ";
    }
    os << var.name << ";\n";
  }
  for (const auto &var : module.unpacked_variables) {
    if (port_names.count(var.name)) {
      continue;
    }
    os << "  ";
    // TODO: we are putting everything into always for now
    // if (var.kind == Module::VariableKind::kWire) {
    //   os << "wire ";
    // }
    os << "logic ";
    if (var.sign) {
      os << "signed ";
    }
    if (var.width > 1) {
      os << "[" << (var.width - 1) << ":0] ";
    }
    os << var.name;
    for (SignalWidth width : var.dims) {
      os << " [" << (width - 1) << ":0]";
    }
    os << ";\n";
  }
  os << "\n";
}

void VerilogEmitter::emit_instances(const Module &module, std::ostream &os) const {
  for (const auto &node : module.nodes) {
    if (node.kind != Module::NodeKind::kInstance) {
      continue;
    }
    const auto &child = design_.modules[node.module_id];
    const std::string inst = node.name;
    os << "  " << child.name << " " << inst << " (\n";
    bool first = true;
    for (size_t i = 0; i < child.input_ports.size(); ++i) {
      if (!first) {
        os << ",\n";
      }
      first = false;
      const auto &p = child.input_ports[i];
      const auto &data_ref = node.inputs[i];
      os << "    ." << p.name << "(";
      if (data_ref.node_id == Tig::kInvalidNodeId) {
        os << "1'bx";
      } else {
        const auto &data_node = module.nodes[data_ref.node_id];
        const std::string data_name = data_node.outputs[data_ref.port_idx].name;
        if (data_name.empty()) { // handle convert
          assert(data_ref.port_idx < data_node.expr_roots.size());
          emit_expr_inline(data_node.expr_graph, data_node.expr_roots[data_ref.port_idx], "", os);
        } else {
          os << data_name;
        }
      }
      os << ")";
    }
    for (size_t i = 0; i < child.output_ports.size(); ++i) {
      if (!first) {
        os << ",\n";
      }
      first = false;
      const auto &p = child.output_ports[i];
      const std::string sig = node.outputs[i].name;
      os << "    ." << p.name << "(" << sig << ")";
    }
    os << "  );\n";
  }
  os << "\n";
}

void VerilogEmitter::emit_combinational(const Module &module, std::ostream &os) const {
  // TODO: latches are not separated yet
  for (const auto &node : module.nodes) {
    if (node.kind == Module::NodeKind::kOp) {
      assert(node.outputs.size() == node.expr_roots.size());
      bool fEmpty = true;
      for (size_t i = 0; i < node.outputs.size(); ++i) {
        if (!node.outputs[i].name.empty()) {
          fEmpty = false;
          break;
        }
      }
      if (fEmpty) {
        continue;
      }
      os << "  always @(*) begin\n";
      for (size_t i = 0; i < node.outputs.size(); ++i) {
        if (!node.outputs[i].name.empty()) { // skip convert (already handled above)
          emit_expr(node.outputs[i].name, false, false, node.expr_graph, node.expr_roots[i], os,
                    "    ");
        }
      }
      os << "  end\n";
    } else if (node.kind == Module::NodeKind::kMerge) {
      assert(node.outputs.size() == 1);
      std::string name = node.outputs[0].name;
      if (!name.empty()) {
        os << "  always @(*) begin\n";
        for (const auto &input : node.inputs) {
          const auto &input_node = module.nodes[input.node_id];
          assert(input.node_id < module.nodes.size());
          assert(input.port_idx < input_node.expr_roots.size());
          if (input_node.kind == Module::NodeKind::kOp) {
            emit_expr(name, false, false, input_node.expr_graph,
                      input_node.expr_roots[input.port_idx], os, "    ");
          } else {
            // TODO: handle multi_driver
          }
        }
        os << "  end\n";
      }
    }
  }
}

void VerilogEmitter::emit_sequential(const Module &module, std::ostream &os) const {
  auto edge_to_string = [](EdgeKind edge) -> const char * {
    switch (edge) {
    case EdgeKind::kPosedge:
      return "posedge";
    case EdgeKind::kNegedge:
      return "negedge";
    case EdgeKind::kBothEdges:
      return "edge";
    case EdgeKind::kNone:
    default:
      throw std::logic_error("Invalid edge kind for sequential emission");
    }
  };
  std::map<Tig::NodeId, std::string> merged_ffs;
  for (const auto &node : module.nodes) {
    if (node.kind == Module::NodeKind::kFfMerge) {
      for (const auto &input : node.inputs) {
        assert(input.port_idx == 0);
        merged_ffs[input.node_id] = node.outputs[0].name;
      }
    }
  }
  const auto emit_ff_data = [&](const auto &self, std::string_view lhs,
                                const Module::EdgeRef &data_ref, std::string_view indent,
                                const std::unordered_map<std::string, bool> *assumptions,
                                bool is_nonblocking, bool is_merge) -> void {
    assert(data_ref.node_id < module.nodes.size());
    const auto &data_node = module.nodes[data_ref.node_id];
    const std::string data_name = data_node.outputs[data_ref.port_idx].name;
    if (!data_name.empty()) {
      os << indent << lhs << ((is_nonblocking && !is_merge) ? " <= " : " = ") << data_name << ";\n";
      return;
    }
    if (data_node.kind == Module::NodeKind::kOp) {
      assert(data_ref.port_idx < data_node.expr_roots.size());
      emit_expr(lhs, is_nonblocking, is_merge, data_node.expr_graph,
                data_node.expr_roots[data_ref.port_idx], os, indent, assumptions);
      return;
    }
    assert(data_node.kind == Module::NodeKind::kMerge);
    for (const auto &input : data_node.inputs) {
      // Merge expansion needs blocking assignments to accumulate writes within this block.
      self(self, lhs, input, indent, assumptions, is_nonblocking, true);
    }
  };
  for (Tig::NodeId ff_id = 0; ff_id < module.nodes.size(); ++ff_id) {
    const auto &node = module.nodes[ff_id];
    if (node.kind != Module::NodeKind::kFf) {
      continue;
    }
    assert(node.inputs.size() == 2 || node.inputs.size() == 3);
    assert(node.outputs.size() == 1);
    std::string lhs_name = node.outputs[0].name;
    if (lhs_name.empty()) {
      auto it = merged_ffs.find(ff_id);
      if (it != merged_ffs.end()) {
        lhs_name = it->second;
      }
    }
    assert(!lhs_name.empty());
    const auto &data_ref = node.inputs[0];
    const auto &clk_ref = node.inputs[1];
    const auto &clk_node = module.nodes[clk_ref.node_id];
    const std::string clk_name = clk_node.outputs[clk_ref.port_idx].name;
    std::string rst_name;
    // TODO: bothedge
    os << "  always @(" << edge_to_string(node.clk_edge) << " " << clk_name;
    if (node.inputs.size() == 3) {
      const auto &rst_ref = node.inputs[2];
      const auto &rst_node = module.nodes[rst_ref.node_id];
      rst_name = rst_node.outputs[rst_ref.port_idx].name;
      os << " or " << edge_to_string(node.rst_edge) << " " << rst_name;
    }
    os << ") begin\n";
    if (!rst_name.empty()) {
      const bool reset_value = node.rst_edge == EdgeKind::kPosedge;
      const std::unordered_map<std::string, bool> reset_assumptions{{rst_name, reset_value}};
      const std::unordered_map<std::string, bool> clock_assumptions{{rst_name, !reset_value}};
      os << "    if (" << ((node.rst_edge == EdgeKind::kNegedge) ? "!" : "") << rst_name
         << ") begin\n";
      // Emit top-level FF writes as NBA so Yosys can prove the sequential memory/FF pattern.
      emit_ff_data(emit_ff_data, lhs_name, data_ref, "      ", &reset_assumptions, true, false);
      os << "    end else begin\n";
      // Emit top-level FF writes as NBA so Yosys can prove the sequential memory/FF pattern.
      emit_ff_data(emit_ff_data, lhs_name, data_ref, "      ", &clock_assumptions, true, false);
      os << "    end\n";
    } else {
      // Emit top-level FF writes as NBA so Yosys can prove the sequential memory/FF pattern.
      emit_ff_data(emit_ff_data, lhs_name, data_ref, "    ", nullptr, true, false);
    }
    os << "  end\n";
  }
}

bool VerilogEmitter::lookup_assumed_condition(
    const ExprGraph &expr_graph, ExprId id,
    const std::unordered_map<std::string, bool> *assumptions, bool &value) const {
  if (assumptions == nullptr) {
    return false;
  }
  if (id == kInvalidExprId) {
    return false;
  }
  const auto &node = expr_graph.nodes[id];
  if (node.op == ExprGraph::Op::kInput) {
    for (const auto &input : expr_graph.inputs) {
      if (input.second == id) {
        auto it = assumptions->find(input.first);
        if (it == assumptions->end()) {
          return false;
        }
        value = it->second;
        return true;
      }
    }
    return false;
  }
  if ((node.op == ExprGraph::Op::kLogicalNot ||
       (node.op == ExprGraph::Op::kBitwiseNot && node.width == 1)) &&
      node.operands.size() == 1 &&
      lookup_assumed_condition(expr_graph, node.operands[0], assumptions, value)) {
    value = !value;
    return true;
  }
  return false;
}

void VerilogEmitter::emit_expr(std::string_view lhs, bool is_nonblocking, bool is_merge,
                               const ExprGraph &expr_graph, ExprId id, std::ostream &os,
                               std::string_view indent,
                               const std::unordered_map<std::string, bool> *assumptions) const {
  std::map<ExprId, std::string> names;
  std::string lhs_name(lhs);
  std::ostringstream decl_os;
  std::ostringstream stmt_os;
  std::ostringstream assign_os;
  const std::string inner_indent = std::string(indent) + "  ";
  emit_expr_unpacked(lhs_name, is_nonblocking, is_merge, expr_graph, id, names, decl_os, stmt_os,
                     assign_os, inner_indent, assumptions);
  const std::string decls = decl_os.str();
  const std::string stmts = stmt_os.str();
  const std::string assigns = assign_os.str();
  if (!decls.empty() || !stmts.empty() || !assigns.empty()) {
    os << indent << "begin\n";
    os << decls;
    os << stmts;
    os << assigns;
    os << indent << "end\n";
  }
}

void VerilogEmitter::emit_expr_unpacked(
    std::string lhs, bool is_nonblocking, bool is_merge, const ExprGraph &expr_graph, ExprId id,
    std::map<ExprId, std::string> &names, std::ostream &decl_os, std::ostream &os,
    std::ostream &assign_os, std::string_view indent,
    const std::unordered_map<std::string, bool> *assumptions) const {
  if (id == kInvalidExprId) {
    return;
  }
  const auto &node = expr_graph.nodes[id];
  switch (node.op) {
  case ExprGraph::Op::kSequence:
    for (ExprId operand : node.operands) {
      emit_expr_unpacked(lhs, is_nonblocking, is_merge, expr_graph, operand, names, decl_os, os,
                         assign_os, indent, assumptions);
    }
    break;
  case ExprGraph::Op::kUnpackedAssign: {
    const ExprId next = node.operands[0];
    const ExprId base = node.operands[1];
    const ExprId slice_width = node.operands[2];
    std::ostringstream selected_lhs;
    selected_lhs << lhs << "["
                 << emit_expr_packed(expr_graph, base, names, decl_os, os, indent, assumptions);
    if (slice_width != expr_graph.constant_one) {
      selected_lhs << " +: "
                   << emit_expr_packed(expr_graph, slice_width, names, decl_os, os, indent,
                                       assumptions);
    }
    selected_lhs << "]";
    emit_expr_unpacked(selected_lhs.str(), is_nonblocking, false, expr_graph, next, names, decl_os,
                       os, assign_os, indent, assumptions);
    break;
  }
  case ExprGraph::Op::kMux: {
    bool assumed = false;
    if (lookup_assumed_condition(expr_graph, node.operands[0], assumptions, assumed)) {
      emit_expr_unpacked(lhs, is_nonblocking, is_merge, expr_graph, node.operands[assumed ? 1 : 2],
                         names, decl_os, os, assign_os, indent, assumptions);
      break;
    }
    const std::string cond =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string branch_indent = std::string(indent) + "  ";
    std::ostringstream then_assign_os;
    emit_expr_unpacked(lhs, is_nonblocking, is_merge, expr_graph, node.operands[1], names, decl_os,
                       os, then_assign_os, branch_indent, assumptions);
    std::ostringstream else_assign_os;
    emit_expr_unpacked(lhs, is_nonblocking, is_merge, expr_graph, node.operands[2], names, decl_os,
                       os, else_assign_os, branch_indent, assumptions);
    assign_os << indent << "if (" << cond << ") begin\n";
    assign_os << then_assign_os.str();
    assign_os << indent << "end else begin\n";
    assign_os << else_assign_os.str();
    assign_os << indent << "end\n";
    break;
  }
  case ExprGraph::Op::kCase: {
    assert(!node.operands.empty());
    const bool has_default = node.operands.size() % 2 == 0;
    const std::string selector =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string branch_indent = std::string(indent) + "  ";
    struct CaseArm {
      std::string label;
      std::string assignments;
    };
    std::vector<CaseArm> arms;
    size_t i = 1;
    while (i + 1 < node.operands.size()) {
      std::ostringstream label;
      const auto &value_node = expr_graph.nodes[node.operands[i]];
      if (value_node.op == ExprGraph::Op::kList) {
        for (size_t k = 0; k < value_node.operands.size(); ++k) {
          if (k) {
            label << ", ";
          }
          label << emit_expr_packed(expr_graph, value_node.operands[k], names, decl_os, os, indent,
                                    assumptions);
        }
      } else {
        label << emit_expr_packed(expr_graph, node.operands[i], names, decl_os, os, indent,
                                  assumptions);
      }
      std::ostringstream arm_assign_os;
      emit_expr_unpacked(lhs, is_nonblocking, is_merge, expr_graph, node.operands[i + 1], names,
                         decl_os, os, arm_assign_os, branch_indent, assumptions);
      arms.push_back(CaseArm{label.str(), arm_assign_os.str()});
      i += 2;
    }
    std::ostringstream default_assign_os;
    if (i < node.operands.size()) {
      emit_expr_unpacked(lhs, is_nonblocking, is_merge, expr_graph, node.operands[i], names,
                         decl_os, os, default_assign_os, branch_indent, assumptions);
    }
    assign_os << indent << "case (" << selector << ")";
    if (!has_default) {
      assign_os << " // synopsys full_case";
    }
    assign_os << "\n";
    for (const CaseArm &arm : arms) {
      assign_os << indent << arm.label << ": begin\n";
      assign_os << arm.assignments;
      assign_os << indent << "end\n";
    }
    if (!default_assign_os.str().empty()) {
      assign_os << indent << "default: begin\n";
      assign_os << default_assign_os.str();
      assign_os << indent << "end\n";
    }
    assign_os << indent << "endcase\n";
    break;
  }
  default: {
    const std::string rhs =
        emit_expr_packed(expr_graph, id, names, decl_os, os, indent, assumptions);
    if (!rhs.empty()) {
      assign_os << indent << lhs << ((is_nonblocking && !is_merge) ? " <= " : " = ") << rhs
                << ";\n";
    }
    break;
  }
  }
}

std::string
VerilogEmitter::emit_expr_packed(const ExprGraph &expr_graph, ExprId id,
                                 std::map<ExprId, std::string> &names, std::ostream &decl_os,
                                 std::ostream &os, std::string_view indent,
                                 const std::unordered_map<std::string, bool> *assumptions) const {
  if (id == kInvalidExprId) {
    return "";
  }
  if (auto it = names.find(id); it != names.end()) {
    return it->second;
  }

  auto temp_name = [&]() { return "_abys_tmp_" + std::to_string(id); };
  auto declare_temp = [&](const ExprGraph::Node &node, std::string_view name) {
    decl_os << indent << "logic ";
    if (node.sign) {
      decl_os << "signed ";
    }
    if (node.width > 1) {
      decl_os << "[" << (node.width - 1) << ":0] ";
    }
    decl_os << name << ";\n";
  };

  const auto &node = expr_graph.nodes[id];
  auto emit_unary = [&](const char *op) {
    const std::string operand =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = (" << op << operand << ");\n";
    names[id] = name;
    return name;
  };
  auto emit_bin = [&](const char *op) {
    const std::string lhs_name =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string rhs_name =
        emit_expr_packed(expr_graph, node.operands[1], names, decl_os, os, indent, assumptions);
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = (" << lhs_name << " " << op << " " << rhs_name << ");\n";
    names[id] = name;
    return name;
  };
  auto emit_variadic = [&](const char *op) {
    std::vector<std::string> operand_names;
    operand_names.reserve(node.operands.size());
    for (ExprId operand : node.operands) {
      operand_names.push_back(
          emit_expr_packed(expr_graph, operand, names, decl_os, os, indent, assumptions));
    }
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = (";
    for (size_t i = 0; i < operand_names.size(); ++i) {
      if (i) {
        os << " " << op << " ";
      }
      os << operand_names[i];
    }
    os << ");\n";
    names[id] = name;
    return name;
  };

  switch (node.op) {
  case ExprGraph::Op::kInput:
    for (const auto &kv : expr_graph.inputs) {
      if (kv.second == id) {
        names[id] = kv.first;
        return kv.first;
      }
    }
    assert(false);
  case ExprGraph::Op::kConst:
    for (const auto &c : expr_graph.constants) {
      if (c.id == id) {
        names[id] = c.value;
        return c.value;
      }
    }
    assert(false);
  case ExprGraph::Op::kLogicalNot: {
    return emit_unary("!");
  }
  case ExprGraph::Op::kBitwiseNot:
    return emit_unary("~");
  case ExprGraph::Op::kAndReduce:
    return emit_unary("&");
  case ExprGraph::Op::kOrReduce:
    return emit_unary("|");
  case ExprGraph::Op::kXorReduce:
    return emit_unary("^");
  case ExprGraph::Op::kUnaryMinus:
    return emit_unary("-");
  case ExprGraph::Op::kAdd:
    return emit_bin("+");
  case ExprGraph::Op::kSub:
    return emit_bin("-");
  case ExprGraph::Op::kMul:
    return emit_bin("*");
  case ExprGraph::Op::kDiv:
    return emit_bin("/");
  case ExprGraph::Op::kMod:
    return emit_bin("%");
  case ExprGraph::Op::kPow:
    return emit_bin("**");
  case ExprGraph::Op::kShl:
    return emit_bin("<<");
  case ExprGraph::Op::kShr:
    return emit_bin(">>");
  case ExprGraph::Op::kAshr: {
    const std::string lhs_name =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string rhs_name =
        emit_expr_packed(expr_graph, node.operands[1], names, decl_os, os, indent, assumptions);
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = ($signed(" << lhs_name << ") >>> " << rhs_name << ");\n";
    names[id] = name;
    return name;
  }
  case ExprGraph::Op::kEq:
    return emit_bin("==");
  case ExprGraph::Op::kLt:
    return emit_bin("<");
  case ExprGraph::Op::kLe:
    return emit_bin("<=");
  case ExprGraph::Op::kAnd:
    return emit_variadic("&");
  case ExprGraph::Op::kOr:
    return emit_variadic("|");
  case ExprGraph::Op::kXor:
    return emit_variadic("^");
  case ExprGraph::Op::kConvert: {
    const std::string operand =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = " << (node.sign ? "$signed(" : "$unsigned(") << operand << ");\n";
    names[id] = name;
    return name;
  }
  case ExprGraph::Op::kConcat: {
    std::vector<std::string> operand_names;
    operand_names.reserve(node.operands.size());
    for (ExprId operand : node.operands) {
      operand_names.push_back(
          emit_expr_packed(expr_graph, operand, names, decl_os, os, indent, assumptions));
    }
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = {";
    for (size_t i = 0; i < operand_names.size(); ++i) {
      if (i) {
        os << ", ";
      }
      os << operand_names[i];
    }
    os << "};\n";
    names[id] = name;
    return name;
  }
  case ExprGraph::Op::kGather: {
    std::vector<std::string> operand_names;
    operand_names.reserve(node.operands.size());
    for (ExprId operand : node.operands) {
      operand_names.push_back(
          emit_expr_packed(expr_graph, operand, names, decl_os, os, indent, assumptions));
    }
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = '{";
    for (size_t i = 0; i < operand_names.size(); ++i) {
      if (i) {
        os << ", ";
      }
      os << operand_names[i];
    }
    os << "};\n";
    names[id] = name;
    return name;
  }
  case ExprGraph::Op::kRange: {
    const std::string data =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string base =
        emit_expr_packed(expr_graph, node.operands[1], names, decl_os, os, indent, assumptions);
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = " << data << "[" << base;
    if (node.width > 1) {
      os << " +: " << node.width;
    }
    os << "];\n";
    names[id] = name;
    return name;
  }
  case ExprGraph::Op::kUnpackedSelect: {
    const std::string data =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string index =
        emit_expr_packed(expr_graph, node.operands[1], names, decl_os, os, indent, assumptions);
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = " << data << "[" << index << "];\n";
    names[id] = name;
    return name;
  }
  case ExprGraph::Op::kUnpackedRange: {
    const std::string data =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string base =
        emit_expr_packed(expr_graph, node.operands[1], names, decl_os, os, indent, assumptions);
    return data + "[" + base + " +: " + std::to_string(node.width) + "]";
  }
  case ExprGraph::Op::kReverse: {
    const ExprId operand_id = node.operands[0];
    const auto &operand_node = expr_graph.nodes[operand_id];
    const std::string operand =
        emit_expr_packed(expr_graph, operand_id, names, decl_os, os, indent, assumptions);
    if (operand_node.width <= 1) {
      names[id] = operand;
      return operand;
    }
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = {";
    for (SignalWidth i = 0; i < operand_node.width; ++i) {
      if (i) {
        os << ", ";
      }
      os << operand << "[" << i << "]";
    }
    os << "};\n";
    names[id] = name;
    return name;
  }
  case ExprGraph::Op::kMux: {
    bool assumed = false;
    if (lookup_assumed_condition(expr_graph, node.operands[0], assumptions, assumed)) {
      const std::string selected = emit_expr_packed(expr_graph, node.operands[assumed ? 1 : 2],
                                                    names, decl_os, os, indent, assumptions);
      names[id] = selected;
      return selected;
    }
    const std::string cond =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string branch_indent = std::string(indent) + "  ";
    const std::string then_name =
        emit_expr_packed(expr_graph, node.operands[1], names, decl_os, os, indent, assumptions);
    const std::string else_name =
        emit_expr_packed(expr_graph, node.operands[2], names, decl_os, os, indent, assumptions);
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << "if (" << cond << ") begin\n";
    if (!then_name.empty()) {
      os << indent << "  " << name << " = " << then_name << ";\n";
    }
    os << indent << "end else begin\n";
    if (!else_name.empty()) {
      os << indent << "  " << name << " = " << else_name << ";\n";
    }
    os << indent << "end\n";
    names[id] = name;
    return name;
  }
  case ExprGraph::Op::kCase: {
    const bool has_default = node.operands.size() % 2 == 0;
    const std::string selector =
        emit_expr_packed(expr_graph, node.operands[0], names, decl_os, os, indent, assumptions);
    const std::string name = temp_name();
    declare_temp(node, name);
    const std::string branch_indent = std::string(indent) + "  ";
    struct CaseArm {
      std::string label;
      std::string data;
    };
    std::vector<CaseArm> arms;
    size_t i = 1;
    while (i + 1 < node.operands.size()) {
      const ExprId value_id = node.operands[i];
      const ExprId data_id = node.operands[i + 1];
      const std::string data_name =
          emit_expr_packed(expr_graph, data_id, names, decl_os, os, indent, assumptions);
      if (!data_name.empty()) {
        std::ostringstream label;
        const auto &value_node = expr_graph.nodes[value_id];
        if (value_node.op == ExprGraph::Op::kList) {
          for (size_t k = 0; k < value_node.operands.size(); ++k) {
            if (k) {
              label << ", ";
            }
            label << emit_expr_packed(expr_graph, value_node.operands[k], names, decl_os, os,
                                      indent, assumptions);
          }
        } else {
          label << emit_expr_packed(expr_graph, value_id, names, decl_os, os, indent, assumptions);
        }
        arms.push_back(CaseArm{label.str(), data_name});
      }
      i += 2;
    }
    std::string default_data;
    if (i < node.operands.size()) {
      default_data =
          emit_expr_packed(expr_graph, node.operands[i], names, decl_os, os, indent, assumptions);
    }
    os << indent << "case (" << selector << ")";
    if (!has_default) {
      os << " // synopsys full_case";
    }
    os << "\n";
    for (const CaseArm &arm : arms) {
      os << indent << arm.label << ": begin\n";
      os << branch_indent << name << " = " << arm.data << ";\n";
      os << indent << "end\n";
    }
    if (!default_data.empty()) {
      os << indent << "default: begin\n";
      os << branch_indent << name << " = " << default_data << ";\n";
      os << indent << "end\n";
    }
    os << indent << "endcase\n";
    names[id] = name;
    return name;
  }
  case ExprGraph::Op::kMaskedAssign: {
    const ExprId current_id = node.operands[0];
    const ExprId next_id = node.operands[1];
    const ExprId base_id = node.operands[2];
    const ExprId slice_width_id = node.operands[3];
    const std::string current =
        emit_expr_packed(expr_graph, current_id, names, decl_os, os, indent, assumptions);
    const std::string next =
        emit_expr_packed(expr_graph, next_id, names, decl_os, os, indent, assumptions);
    const std::string base =
        emit_expr_packed(expr_graph, base_id, names, decl_os, os, indent, assumptions);
    const std::string slice_width =
        emit_expr_packed(expr_graph, slice_width_id, names, decl_os, os, indent, assumptions);
    const std::string name = temp_name();
    declare_temp(node, name);
    os << indent << name << " = " << current << ";\n";
    os << indent << name << "[" << base;
    if (slice_width_id != expr_graph.constant_one) {
      os << " +: " << slice_width;
    }
    os << "] = " << next << ";\n";
    names[id] = name;
    return name;
  }
  default:
    throw std::logic_error("Unsupported expression in new Verilog emitter");
  }
}

void VerilogEmitter::emit_expr_inline(
    const ExprGraph &expr_graph, ExprId id, std::string_view lhs, std::ostream &os,
    const std::unordered_map<std::string, bool> *assumptions) const {
  if (id == kInvalidExprId) {
    os << lhs;
    return;
  }
  const auto &node = expr_graph.nodes[id];
  auto emit_bin = [&](const char *op) { // TODO: sign is probably not handled properly
    os << "(";
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << " " << op << " ";
    emit_expr_inline(expr_graph, node.operands[1], lhs, os, assumptions);
    os << ")";
  };
  switch (node.op) {
  case ExprGraph::Op::kInput: { // TODO: use map
    for (const auto &kv : expr_graph.inputs) {
      if (kv.second == id) {
        os << kv.first;
        return;
      }
    }
    assert(false);
  }
  case ExprGraph::Op::kConst: { // TODO: use map
    for (const auto &c : expr_graph.constants) {
      if (c.id == id) {
        os << c.value;
        return;
      }
    }
    assert(false);
  }
  case ExprGraph::Op::kLogicalNot:
    os << "(!";
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << ")";
    return;
  case ExprGraph::Op::kBitwiseNot:
    os << "(~";
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << ")";
    return;
  case ExprGraph::Op::kAndReduce:
    os << "(&";
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << ")";
    return;
  case ExprGraph::Op::kOrReduce:
    os << "(|";
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << ")";
    return;
  case ExprGraph::Op::kXorReduce:
    os << "(^";
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << ")";
    return;
  case ExprGraph::Op::kUnaryMinus:
    os << "(-";
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << ")";
    return;
  case ExprGraph::Op::kAdd:
    emit_bin("+");
    return;
  case ExprGraph::Op::kSub:
    emit_bin("-");
    return;
  case ExprGraph::Op::kMul:
    emit_bin("*");
    return;
  case ExprGraph::Op::kDiv:
    emit_bin("/");
    return;
  case ExprGraph::Op::kMod:
    emit_bin("%");
    return;
  case ExprGraph::Op::kPow:
    emit_bin("**");
    return;
  case ExprGraph::Op::kShl:
    emit_bin("<<");
    return;
  case ExprGraph::Op::kShr:
    emit_bin(">>");
    return;
  case ExprGraph::Op::kAshr:
    os << "($signed(";
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << ") >>> ";
    emit_expr_inline(expr_graph, node.operands[1], lhs, os, assumptions);
    os << ")";
    return;
  case ExprGraph::Op::kEq:
    emit_bin("==");
    return;
  case ExprGraph::Op::kLt:
    emit_bin("<");
    return;
  case ExprGraph::Op::kLe:
    emit_bin("<=");
    return;
  case ExprGraph::Op::kAnd:
  case ExprGraph::Op::kOr:
  case ExprGraph::Op::kXor: {
    const char *op = (node.op == ExprGraph::Op::kAnd)  ? "&"
                     : (node.op == ExprGraph::Op::kOr) ? "|"
                                                       : "^";
    os << "(";
    for (size_t i = 0; i < node.operands.size(); ++i) {
      if (i) {
        os << " " << op << " ";
      }
      emit_expr_inline(expr_graph, node.operands[i], lhs, os, assumptions);
    }
    os << ")";
    return;
  }
  case ExprGraph::Op::kMux: {
    bool assumed = false;
    if (lookup_assumed_condition(expr_graph, node.operands[0], assumptions, assumed)) {
      emit_expr_inline(expr_graph, node.operands[assumed ? 1 : 2], lhs, os, assumptions);
      return;
    }
    os << "(";
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << " ? ";
    emit_expr_inline(expr_graph, node.operands[1], lhs, os, assumptions);
    os << " : ";
    emit_expr_inline(expr_graph, node.operands[2], lhs, os, assumptions);
    os << ")";
    return;
  }
  case ExprGraph::Op::kCase: {
    // operands = [selector, value0, data0, value1, data1, ..., default?]
    const ExprId sel = node.operands[0];
    auto emit_match = [&](ExprId value_id) {
      const auto &v = expr_graph.nodes[value_id];
      if (v.op == ExprGraph::Op::kList) {
        os << "(";
        for (size_t k = 0; k < v.operands.size(); ++k) {
          if (k) {
            os << " || ";
          }
          os << "(";
          emit_expr_inline(expr_graph, sel, lhs, os, assumptions);
          os << " == ";
          emit_expr_inline(expr_graph, v.operands[k], lhs, os, assumptions);
          os << ")";
        }
        os << ")";
      } else {
        os << "(";
        emit_expr_inline(expr_graph, sel, lhs, os, assumptions);
        os << " == ";
        emit_expr_inline(expr_graph, value_id, lhs, os, assumptions);
        os << ")";
      }
    };
    size_t i = 1;
    bool first = true;
    while (i + 1 < node.operands.size()) {
      const ExprId value_id = node.operands[i++];
      const ExprId data_id = node.operands[i++];
      if (!first) {
        os << " : ";
      }
      first = false;
      emit_match(value_id);
      os << " ? ";
      emit_expr_inline(expr_graph, data_id, lhs, os, assumptions);
    }
    if (i < node.operands.size()) {
      if (!first) {
        os << " : ";
      }
      emit_expr_inline(expr_graph, node.operands[i], lhs, os, assumptions);
    } else {
      if (!first) {
        os << " : ";
      }
      // no default -> unknown
      os << lhs;
    }
    return;
  }
  case ExprGraph::Op::kConvert: {
    os << (node.sign ? "$signed(" : "$unsigned(");
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << ")";
    return;
  }
  case ExprGraph::Op::kConcat:
    os << "{";
    for (size_t i = 0; i < node.operands.size(); ++i) {
      if (i) {
        os << ", ";
      }
      emit_expr_inline(expr_graph, node.operands[i], lhs, os, assumptions);
    }
    os << "}";
    return;
  case ExprGraph::Op::kReverse: {
    const ExprId op_id = node.operands[0];
    const auto &op_node = expr_graph.nodes[op_id];
    if (op_node.width <= 1) {
      emit_expr_inline(expr_graph, op_id, lhs, os, assumptions);
      return;
    }
    os << "{";
    for (SignalWidth i = 0; i < op_node.width; ++i) {
      if (i != 0) {
        os << ", ";
      }
      if (!kUseShiftMaskForExpressionSelects || can_emit_direct_range_base(expr_graph, op_id)) {
        emit_expr_inline(expr_graph, op_id, lhs, os, assumptions);
        os << "[" << i << "]";
      } else {
        os << "((";
        emit_expr_inline(expr_graph, op_id, lhs, os, assumptions);
        os << " >> " << i << ") & 1'b1)";
      }
    }
    os << "}";
    return;
  }
  case ExprGraph::Op::kRange: {
    const ExprId data_id = node.operands[0];
    const ExprId base_id = node.operands[1];
    if (!kUseShiftMaskForExpressionSelects || can_emit_direct_range_base(expr_graph, data_id)) {
      emit_expr_inline(expr_graph, data_id, lhs, os, assumptions);
      os << "[";
      emit_expr_inline(expr_graph, base_id, lhs, os, assumptions);
      if (node.width > 1) {
        os << " +: " << node.width;
      }
      os << "]";
    } else {
      emit_shifted_range(expr_graph, data_id, base_id, node.width, lhs, os, assumptions);
    }
    return;
  }
  case ExprGraph::Op::kUnpackedSelect:
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << "[";
    emit_expr_inline(expr_graph, node.operands[1], lhs, os, assumptions);
    os << "]";
    return;
  case ExprGraph::Op::kUnpackedRange:
    emit_expr_inline(expr_graph, node.operands[0], lhs, os, assumptions);
    os << "[";
    emit_expr_inline(expr_graph, node.operands[1], lhs, os, assumptions);
    os << " +: " << node.width << "]";
    return;
  case ExprGraph::Op::kGather: {
    os << "'{";
    for (size_t i = 0; i < node.operands.size(); ++i) {
      if (i) {
        os << ", ";
      }
      emit_expr_inline(expr_graph, node.operands[i], lhs, os, assumptions);
    }
    os << "}";
    return;
  }
  case ExprGraph::Op::kMaskedAssign: { // this is only packed array
    const ExprId current = node.operands[0];
    const ExprId next = node.operands[1];
    const ExprId base = node.operands[2];
    const ExprId slice_width = node.operands[3];
    const SignalWidth width = node.width;
    os << "((";
    emit_expr_inline(expr_graph, current, lhs, os, assumptions);
    os << " & ~(({" << width << "{1'b1}} >> (" << width << " - ";
    emit_expr_inline(expr_graph, slice_width, lhs, os, assumptions);
    os << ")) << (";
    emit_expr_inline(expr_graph, base, lhs, os, assumptions);
    os << "))) | ((";
    emit_expr_inline(expr_graph, next, lhs, os, assumptions);
    os << " & ({" << width << "{1'b1}} >> (" << width << " - ";
    emit_expr_inline(expr_graph, slice_width, lhs, os, assumptions);
    os << "))) << (";
    emit_expr_inline(expr_graph, base, lhs, os, assumptions);
    os << ")))";
    return;
  }
  default:
    // kBothEdge
    assert(false);
  }
}

bool VerilogEmitter::can_emit_direct_range_base(const ExprGraph &expr_graph, ExprId id) const {
  if (id == kInvalidExprId) {
    return false;
  }
  const auto &node = expr_graph.nodes[id];
  return node.op == ExprGraph::Op::kInput || node.op == ExprGraph::Op::kUnpackedSelect;
}

void VerilogEmitter::emit_shifted_range(
    const ExprGraph &expr_graph, ExprId data_id, ExprId base_id, SignalWidth width,
    std::string_view lhs, std::ostream &os,
    const std::unordered_map<std::string, bool> *assumptions) const {
  assert(width > 0);
  os << "((";
  emit_expr_inline(expr_graph, data_id, lhs, os, assumptions);
  os << " >> (";
  emit_expr_inline(expr_graph, base_id, lhs, os, assumptions);
  os << ")) & ";
  if (width == 1) {
    os << "1'b1";
  } else {
    os << width << "'h";
    const int digits = static_cast<int>((width + 3) / 4);
    for (int i = 0; i < digits; ++i) {
      os << "f";
    }
  }
  os << ")";
}

void VerilogEmitter::emit_module_footer(std::ostream &os) const {
  os << "endmodule\n";
}

} // namespace abys::ir
