#include <cassert>
#include <unordered_set>

#include "abys/ir/verilog_emitter.h"

namespace abys::ir {

  VerilogEmitter::VerilogEmitter(const Tig &design) : design_(design) {}

  void VerilogEmitter::emit(std::ostream &os) const {
    bool first = true;
    for (const Module &module: design_.modules) {
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
    for (const auto &var : module.packed_variables) {
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
        os << " [0:" << (width - 1) << "]";
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
      for (size_t i = 0; i < child.input_ports.size(); i++) {
        if (!first) {
          os << ",\n";
        }
        first = false;
        const auto& p = child.input_ports[i];
        const auto& dref = node.inputs[i];
        const auto &d_node = module.nodes[dref.node_id];        
        os << "    ." << p.name << "(";
        if (d_node.kind == Module::NodeKind::kOp) {
          assert(dref.port_idx < d_node.expr_roots.size());
          emit_expr_rec(d_node.expr_graph, d_node.expr_roots[dref.port_idx], os);
        } else {
          const std::string d_name = d_node.outputs[dref.port_idx].name;
          os << d_name;
        }
        os << ")";
      }
      for (size_t i = 0; i < child.output_ports.size(); i++) {
        if (!first) {
          os << ",\n";
        }
        first = false;
        const auto& p = child.output_ports[i];
        const std::string sig = node.outputs[i].name;
        os << "    ." << p.name << "(" << sig << ")";
      }
      os << "  );\n";
    }
    os << "\n";
  }

  void VerilogEmitter::emit_combinational(const Module &module, std::ostream &os) const {
    for (const auto &node : module.nodes) {
      if (node.kind != Module::NodeKind::kOp) {
        continue;
      }
      assert(node.outputs.size() == node.expr_roots.size());
      // TODO: continuous assign with index/range lhs is not supported yet
      // TODO: latches are not separated yet
      os << "  always @(*) begin\n";
      for (size_t i = 0; i < node.outputs.size(); i++) {
        if (node.outputs[i].name.empty()) { // handle convert
          continue;
        }
        os << "    ";
        emit_expr(node.outputs[i].name, false, node.expr_graph, node.expr_roots[i], os);
        os << ";\n";
      }
      os << "  end\n";
    }
  }

  void VerilogEmitter::emit_sequential(const Module &module, std::ostream &os) const {
    for (const auto &node : module.nodes) {
      if (node.kind != Module::NodeKind::kFf) {
        continue;
      }
      assert(node.inputs.size() == 2);
      assert(node.outputs.size() == 1);
      const auto &dref   = node.inputs[0];
      const auto &d_node = module.nodes[dref.node_id];
      const auto &clkref = node.inputs[1];
      const auto &clk_node = module.nodes[clkref.node_id];
      const std::string clk_name = clk_node.outputs[clkref.port_idx].name;
      // TODO: bothedge
      os << "  always @(posedge " << clk_name << ") begin\n";
      os << "    ";
      if (d_node.kind == Module::NodeKind::kOp) {
        assert(dref.port_idx < d_node.expr_roots.size());
        emit_expr(node.outputs[0].name, true, d_node.expr_graph, d_node.expr_roots[dref.port_idx], os);
      } else {
        const std::string d_name = d_node.outputs[dref.port_idx].name;
        os << node.outputs[0].name << " <= " << d_name;
      }
      os << ";\n";
      os << "  end\n";
    }
  }

  void VerilogEmitter::emit_expr(const std::string &name, bool nonblocking, const ExprGraph &expr_graph, ExprId root, std::ostream &os) const {
    os << name;
    if (nonblocking) {
      os << " <= ";
    } else {
      os << " = ";
    }
    emit_expr_rec(expr_graph, root, os);
  }

 void VerilogEmitter::emit_expr_rec(const ExprGraph &expr_graph, ExprId id, std::ostream &os) const {
    assert(id != kInvalidExprId);
    const auto &node = expr_graph.nodes[id];
    auto emit_bin = [&](const char *op) { // TODO: sign is probably not handled properly
      os << "(";
      emit_expr_rec(expr_graph, node.operands[0], os);
      os << " " << op << " ";
      emit_expr_rec(expr_graph, node.operands[1], os);
      os << ")";
    };
    switch (node.op) {
    case ExprGraph::Op::kInput: { //TODO: use map
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
      os << "(!"; emit_expr_rec(expr_graph, node.operands[0], os); os << ")";
      return;
    case ExprGraph::Op::kBitwiseNot:
      os << "(~"; emit_expr_rec(expr_graph, node.operands[0], os); os << ")";
      return;
    case ExprGraph::Op::kAndReduce:
      os << "(&";
      emit_expr_rec(expr_graph, node.operands[0], os);
      os << ")";
      return;
    case ExprGraph::Op::kOrReduce:
      os << "(|";
      emit_expr_rec(expr_graph, node.operands[0], os);
      os << ")";
      return;
    case ExprGraph::Op::kXorReduce:
      os << "(^";
      emit_expr_rec(expr_graph, node.operands[0], os);
      os << ")";
      return;
    case ExprGraph::Op::kUnaryMinus:
      os << "(-"; emit_expr_rec(expr_graph, node.operands[0], os); os << ")";
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
    case ExprGraph::Op::kShl:
      emit_bin("<<");
      return;
    case ExprGraph::Op::kShr:
      emit_bin(">>");
      return;
    case ExprGraph::Op::kAshr:
      os << "($signed(";
      emit_expr_rec(expr_graph, node.operands[0], os);
      os << ") >>> ";
      emit_expr_rec(expr_graph, node.operands[1], os);
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
      const char *op = (node.op == ExprGraph::Op::kAnd) ? "&" : (node.op == ExprGraph::Op::kOr)  ? "|" : "^";
      os << "(";
      for (size_t i = 0; i < node.operands.size(); i++) {
        if (i) {
          os << " " << op << " ";
        }
        emit_expr_rec(expr_graph, node.operands[i], os);
      }
      os << ")";
      return;
    }
    case ExprGraph::Op::kMux:
      os << "(";
      emit_expr_rec(expr_graph, node.operands[0], os);
      os << " ? ";
      emit_expr_rec(expr_graph, node.operands[1], os);
      os << " : ";
      emit_expr_rec(expr_graph, node.operands[2], os);
      os << ")";
      return;
    case ExprGraph::Op::kConvert: {
      os << (node.sign ? "$signed(" : "$unsigned(");
      emit_expr_rec(expr_graph, node.operands[0], os);
      os << ")";
      return;
    }
    case ExprGraph::Op::kConcat:
      os << "{";
      for (size_t i = 0; i < node.operands.size(); i++) {
        if (i) {
          os << ", ";
        }
        emit_expr_rec(expr_graph, node.operands[i], os);
      }
      os << "}";
      return;
    case ExprGraph::Op::kReverse: {
      const ExprId op_id = node.operands[0];
      const auto &op_node = expr_graph.nodes[op_id];
      if (op_node.width <= 1) {
        emit_expr_rec(expr_graph, op_id, os);
        return;
      }
      os << "{";
      for (SignalWidth i = 0; i < op_node.width; i++) {
        if (i != 0) {
          os << ", ";
        }
        emit_expr_rec(expr_graph, op_id, os);
        os << "[" << i << "]";
      }
      os << "}";
      return;
    }
    case ExprGraph::Op::kRange:
      emit_expr_rec(expr_graph, node.operands[0], os);
      os << "[";
      emit_expr_rec(expr_graph, node.operands[1], os);
      if (node.width > 1) {
        os << " -: " << node.width;
      }
      os << "]";
      return;
    case ExprGraph::Op::kArraySelect:
      emit_expr_rec(expr_graph, node.operands[0], os);
      os << "[";
      emit_expr_rec(expr_graph, node.operands[1], os);
      os << "]";
      return;
    case ExprGraph::Op::kMaskedAssign: {
      const ExprId current = node.operands[0];
      const ExprId next = node.operands[1];
      const ExprId base = node.operands[2];
      const ExprId slice = node.operands[3];
      const SignalWidth W = node.width;
      os << "((";
      emit_expr_rec(expr_graph, current, os);
      os << " & ~(({" << W << "{1'b1}} >> (" << W << " - ";
      emit_expr_rec(expr_graph, slice, os);
      os << ")) << ";
      emit_expr_rec(expr_graph, base, os);
      os << ")) | ((";
      emit_expr_rec(expr_graph, next, os);
      os << " & ({" << W << "{1'b1}} >> (" << W << " - ";
      emit_expr_rec(expr_graph, slice, os);
      os << "))) << ";
      emit_expr_rec(expr_graph, base, os);
      os << "))";
      return;
    }
    default:
      //kCase (and kList handling inside it)
      //kBothEdge
      assert(false);
    }
  }

  void VerilogEmitter::emit_module_footer(std::ostream &os) const {
    os << "endmodule\n";
  }

} // namespace abys::ir
