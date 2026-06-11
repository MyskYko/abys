#include <cassert>
#include <stdexcept>
#include <unordered_set>
#include <map>
#include <sstream>
#include <vector>

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
            emit_expr_rec(data_node.expr_graph, data_node.expr_roots[data_ref.port_idx], "", os);
          } else {
            os << data_name;
          }
        }
        os << ")";
      }
      for (size_t i = 0; i < child.output_ports.size(); i++) {
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
        for (size_t i = 0; i < node.outputs.size(); i++) {
          if (!node.outputs[i].name.empty()) {
            fEmpty = false;
            break;
          }
        }
        if (fEmpty) {
          continue;
        }
        os << "  always @(*) begin\n";
        for (size_t i = 0; i < node.outputs.size(); i++) {
          if (!node.outputs[i].name.empty()) { // skip convert (already handled above)
            emit_expr(node.outputs[i].name, false, node.expr_graph, node.expr_roots[i], os, "    ");
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
            if(input_node.kind == Module::NodeKind::kOp) {
              emit_expr(name, false, input_node.expr_graph, input_node.expr_roots[input.port_idx], os, "    ");
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
    for (Tig::NodeId ff_id = 0; ff_id < module.nodes.size(); ff_id++) {
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
      const auto &data_node = module.nodes[data_ref.node_id];
      const std::string data_name = data_node.outputs[data_ref.port_idx].name;
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
      if (data_name.empty()) {
        if (data_node.kind == Module::NodeKind::kOp) {
          assert(data_ref.port_idx < data_node.expr_roots.size());
          emit_expr(lhs_name, true, data_node.expr_graph, data_node.expr_roots[data_ref.port_idx], os, "    ");
        } else if (data_node.kind == Module::NodeKind::kMerge) {
          if (!rst_name.empty()) {
            emit_reset_mux_merge(lhs_name, module, data_node, rst_name, node.rst_edge, os, "    ");
          } else {
            for (const auto &input : data_node.inputs) {
              const auto &input_node = module.nodes[input.node_id];
              assert(input.node_id < module.nodes.size());
              assert(input_node.kind == Module::NodeKind::kOp);
              assert(input.port_idx < input_node.expr_roots.size());
              emit_expr(lhs_name, true, input_node.expr_graph, input_node.expr_roots[input.port_idx], os, "    ");
            }
          }
        }
      } else {
        os << "    " << node.outputs[0].name << " <= " << data_name << ";\n";
      }
      os << "  end\n";
    }
  }

  void VerilogEmitter::emit_reset_mux_merge(std::string_view lhs, const Module &module, const Module::Node &merge_node, std::string_view rst_name, EdgeKind rst_edge, std::ostream &os, std::string_view indent) const {
    struct ResetMuxBranches {
      const ExprGraph *expr_graph;
      ExprId on_reset_id;
      ExprId on_clock_id;
    };
    std::vector<ResetMuxBranches> branches;
    branches.reserve(merge_node.inputs.size());
    const std::string expected_cond = (rst_edge == EdgeKind::kNegedge) ? "(!" + std::string(rst_name) + ")" : std::string(rst_name);
    for (const auto &input : merge_node.inputs) {
      const auto &input_node = module.nodes[input.node_id];
      assert(input.node_id < module.nodes.size());
      assert(input_node.kind == Module::NodeKind::kOp);
      assert(input.port_idx < input_node.expr_roots.size());
      const ExprId root_id = input_node.expr_roots[input.port_idx];
      const auto &root = input_node.expr_graph.nodes[root_id];
      assert(root.op == ExprGraph::Op::kMux);
      std::ostringstream cond_ss;
      emit_expr_rec(input_node.expr_graph, root.operands[0], "", cond_ss);
      assert(cond_ss.str() == expected_cond);
      branches.push_back({&input_node.expr_graph, root.operands[1], root.operands[2]});
    }
    os << indent << "if (" << ((rst_edge == EdgeKind::kNegedge) ? "!" : "") << rst_name << ") begin\n";
    for (const auto &branch : branches) {
      emit_expr(lhs, true, *branch.expr_graph, branch.on_reset_id, os, std::string(indent) + "  ");
    }
    os << indent << "end else begin\n";
    for (const auto &branch : branches) {
      emit_expr(lhs, true, *branch.expr_graph, branch.on_clock_id, os, std::string(indent) + "  ");
    }
    os << indent << "end\n";
  }

  void VerilogEmitter::emit_expr(std::string_view lhs, bool nonblocking, const ExprGraph &expr_graph, ExprId id, std::ostream &os, std::string_view indent) const {
    if (id == kInvalidExprId) {
      return;
    }
    const auto &node = expr_graph.nodes[id];
    switch (node.op) {
    case ExprGraph::Op::kMaskedAssign: {
      const ExprId current = node.operands[0];
      const ExprId next = node.operands[1];
      const ExprId base = node.operands[2];
      const ExprId slice_width = node.operands[3];
      emit_expr(lhs, false, expr_graph, current, os, indent); // turn off nonblocking to permit cascaded masked assigns
      std::ostringstream ss;
      ss << lhs << "[";
      emit_expr_rec(expr_graph, base, "", ss);
      if (slice_width != expr_graph.constant_one) {
        ss << " +: ";
        emit_expr_rec(expr_graph, slice_width, "", ss);
      }
      ss << "]";
      emit_expr(ss.str(), nonblocking && current == kInvalidExprId, expr_graph, next, os, indent);
      return;
    }
    case ExprGraph::Op::kMux:
      os << indent << "if (";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << ") begin\n";
      emit_expr(lhs, nonblocking, expr_graph, node.operands[1], os, std::string(indent) + "  ");
      os << indent << "end else begin\n";
      emit_expr(lhs, nonblocking, expr_graph, node.operands[2], os, std::string(indent) + "  ");
      os << indent << "end\n";
      return;
    case ExprGraph::Op::kCase: {
      assert(!node.operands.empty());
      os << indent << "case (";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << ")\n";
      size_t i = 1;
      while (i + 1 < node.operands.size()) {
        os << indent;
        const auto &value_node = expr_graph.nodes[node.operands[i]];
        if (value_node.op == ExprGraph::Op::kList) {
          for (size_t k = 0; k < value_node.operands.size(); k++) {
            if (k) {
              os << ", ";
            }
            emit_expr_rec(expr_graph, value_node.operands[k], lhs, os);
          }
        } else {
          emit_expr_rec(expr_graph, node.operands[i], lhs, os);
        }
        os << ": begin\n";
        emit_expr(lhs, nonblocking, expr_graph, node.operands[i + 1], os, std::string(indent) + "  ");
        os << indent << "end\n";
        i += 2;
      }
      if (i < node.operands.size()) {
        os << indent << "default: begin\n";
        emit_expr(lhs, nonblocking, expr_graph, node.operands[i], os, std::string(indent) + "  ");
        os << indent << "end\n";
      }
      os << indent << "endcase\n";
      return;
    }
    default:
      os << indent << lhs;
      os << (nonblocking ? " <= " : " = ");
      emit_expr_rec(expr_graph, id, lhs, os);
      os << ";\n";
      return;
    }
  }

  void VerilogEmitter::emit_expr_rec(const ExprGraph &expr_graph, ExprId id, std::string_view lhs, std::ostream &os) const {
    if (id == kInvalidExprId) {
      os << lhs;
      return;
    }
    const auto &node = expr_graph.nodes[id];
    auto emit_bin = [&](const char *op) { // TODO: sign is probably not handled properly
      os << "(";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << " " << op << " ";
      emit_expr_rec(expr_graph, node.operands[1], lhs, os);
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
      os << "(!";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << ")";
      return;
    case ExprGraph::Op::kBitwiseNot:
      os << "(~";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << ")";
      return;
    case ExprGraph::Op::kAndReduce:
      os << "(&";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << ")";
      return;
    case ExprGraph::Op::kOrReduce:
      os << "(|";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << ")";
      return;
    case ExprGraph::Op::kXorReduce:
      os << "(^";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << ")";
      return;
    case ExprGraph::Op::kUnaryMinus:
      os << "(-";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
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
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << ") >>> ";
      emit_expr_rec(expr_graph, node.operands[1], lhs, os);
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
        emit_expr_rec(expr_graph, node.operands[i], lhs, os);
      }
      os << ")";
      return;
    }
    case ExprGraph::Op::kMux:
      os << "(";
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << " ? ";
      emit_expr_rec(expr_graph, node.operands[1], lhs, os);
      os << " : ";
      emit_expr_rec(expr_graph, node.operands[2], lhs, os);
      os << ")";
      return;
    case ExprGraph::Op::kCase: {
      // operands = [selector, value0, data0, value1, data1, ..., default?]
      const ExprId sel = node.operands[0];
      auto emit_match = [&](ExprId value_id) {
        const auto &v = expr_graph.nodes[value_id];
        if (v.op == ExprGraph::Op::kList) {
          os << "(";
          for (size_t k = 0; k < v.operands.size(); k++) {
            if (k) {
              os << " || ";
            }
            os << "(";
            emit_expr_rec(expr_graph, sel, lhs, os);
            os << " == ";
            emit_expr_rec(expr_graph, v.operands[k], lhs, os);
            os << ")";
          }
          os << ")";
        } else {
          os << "(";
          emit_expr_rec(expr_graph, sel, lhs, os);
          os << " == ";
          emit_expr_rec(expr_graph, value_id, lhs, os);
          os << ")";
        }
      };
      size_t i = 1;
      bool first = true;
      while (i + 1 < node.operands.size()) {
        const ExprId value_id = node.operands[i++];
        const ExprId data_id  = node.operands[i++];
        if (!first) {
          os << " : ";
        }
        first = false;
        emit_match(value_id);
        os << " ? ";
        emit_expr_rec(expr_graph, data_id, lhs, os);
      }
      if (i < node.operands.size()) {
        if (!first) {
          os << " : ";
        }
        emit_expr_rec(expr_graph, node.operands[i], lhs, os);
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
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << ")";
      return;
    }
    case ExprGraph::Op::kConcat:
      os << "{";
      for (size_t i = 0; i < node.operands.size(); i++) {
        if (i) {
          os << ", ";
        }
        emit_expr_rec(expr_graph, node.operands[i], lhs, os);
      }
      os << "}";
      return;
    case ExprGraph::Op::kReverse: {
      const ExprId op_id = node.operands[0];
      const auto &op_node = expr_graph.nodes[op_id];
      if (op_node.width <= 1) {
        emit_expr_rec(expr_graph, op_id, lhs, os);
        return;
      }
      os << "{";
      for (SignalWidth i = 0; i < op_node.width; i++) {
        if (i != 0) {
          os << ", ";
        }
        emit_expr_rec(expr_graph, op_id, lhs, os);
        os << "[" << i << "]";
      }
      os << "}";
      return;
    }
    case ExprGraph::Op::kRange:
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << "[";
      emit_expr_rec(expr_graph, node.operands[1], lhs, os);
      if (node.width > 1) {
        os << " -: " << node.width;
      }
      os << "]";
      return;
    case ExprGraph::Op::kArraySelect:
      emit_expr_rec(expr_graph, node.operands[0], lhs, os);
      os << "[";
      emit_expr_rec(expr_graph, node.operands[1], lhs, os);
      os << "]";
      return;
    case ExprGraph::Op::kGather: {
      os << "'{";
      for (size_t i = 0; i < node.operands.size(); i++) {
        if (i) {
          os << ", ";
        }
        emit_expr_rec(expr_graph, node.operands[i], lhs, os);
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
      emit_expr_rec(expr_graph, current, lhs, os);
      os << " & ~(({" << width << "{1'b1}} >> (" << width << " - ";
      emit_expr_rec(expr_graph, slice_width, lhs, os);
      os << ")) << ";
      emit_expr_rec(expr_graph, base, lhs, os);
      os << ")) | ((";
      emit_expr_rec(expr_graph, next, lhs, os);
      os << " & ({" << width << "{1'b1}} >> (" << width << " - ";
      emit_expr_rec(expr_graph, slice_width, lhs, os);
      os << "))) << ";
      emit_expr_rec(expr_graph, base, lhs, os);
      os << "))";
      return;
    }
    default:
      //kBothEdge
      assert(false);
    }
  }

  void VerilogEmitter::emit_module_footer(std::ostream &os) const {
    os << "endmodule\n";
  }

} // namespace abys::ir
