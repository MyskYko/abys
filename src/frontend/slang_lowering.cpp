#include "abys/frontend/slang_lowering.h"
#include "slang_lowering_internal.h"

namespace abys::frontend {

class SlangLoweringVisitor final
    : public slang::ast::ASTVisitor<SlangLoweringVisitor, false, false, false, true> {
private:
  using ModuleId = TigBuilder::ModuleId;
  using NodeId = TigBuilder::NodeId;
  static constexpr ModuleId kInvalidModuleId = TigBuilder::kInvalidModuleId;
  static constexpr NodeId kInvalidNodeId = TigBuilder::kInvalidNodeId;
  using Signal = TigBuilder::Signal;
  using SignalSpec = TigBuilder::SignalSpec;

  TigBuilder &builder_;
  const PragmaMap &pragmas_;

  std::vector<ModuleId> module_stack_;
  std::unordered_map<const slang::ast::InstanceBodySymbol *, ModuleId> module_ids_;

  std::string suffix_;
  SlangLoweringContext context_;

  ModuleId current_module_id() const {
    if (module_stack_.empty()) {
      return kInvalidModuleId;
    }
    return module_stack_.back();
  }

  NodeId create_expr_node(const slang::ast::Expression &expr, std::string output_name = "") {
    const ModuleId module_id = current_module_id();
    const NodeId node_id = builder_.create_operation(module_id);
    ExprBuilder expr_builder(builder_.get_expr_graph(module_id, node_id));
    const ExprId expr_id = build_expr(expr, expr_builder, context_);
    expr_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
      builder_.add_node_input_spec(module_id, node_id, name, width, sign);
    });
    builder_.add_node_output_expr(module_id, node_id, std::move(output_name), expr_id, true);
    return node_id;
  }

  std::string create_signal(const slang::ast::ValueSymbol &symbol) {
    const auto &type = symbol.getType().getCanonicalType();
    SignalType signal_type = get_signal_type(type);
    std::string name = register_symbol_name(symbol, context_.special_symbols, suffix_);
    builder_.create_signal(current_module_id(), name, signal_type.width, signal_type.sign,
                           std::move(signal_type.unpacked_dims));
    return name;
  }

public:
  explicit SlangLoweringVisitor(TigBuilder &builder, const PragmaMap &pragmas)
      : builder_(builder), pragmas_(pragmas) {}

private:
  std::string extract_output_named_value(const slang::ast::Expression &expr) {
    assert(expr.kind == slang::ast::ExpressionKind::Assignment);
    const auto &assign = expr.as<slang::ast::AssignmentExpression>();
    assert(assign.right().kind == slang::ast::ExpressionKind::EmptyArgument);
    return extract_named_value(assign.left(), context_.special_symbols);
  }

public:
  template <typename T> void handle(const T &) {
    throw std::logic_error(std::string("Unhandled AST node: ") + typeid(T).name());
  }

  void handle(const slang::ast::SpecifyBlockSymbol &) {}

  void handle(const slang::ast::DefParamSymbol &) {}

  void handle(const slang::ast::GenvarSymbol &) {}

  void handle(const slang::ast::ParameterSymbol &) {
    // TODO: preserve elaborated parameter names and values as module-variant metadata.
  }

  void handle(const slang::ast::PortSymbol &symbol) {
    this->visitDefault(symbol);
    if (symbol.direction == slang::ast::ArgumentDirection::InOut) {
      throw std::logic_error("InOut ports are not supported");
    }
    if (symbol.direction == slang::ast::ArgumentDirection::Ref) {
      throw std::logic_error("Ref ports are not supported");
    }
    const SignalType signal_type = get_signal_type(symbol.getType());
    if (symbol.direction == slang::ast::ArgumentDirection::In) {
      NodeId node_id = builder_.create_module_input(
          current_module_id(), std::string(symbol.name), signal_type.width, signal_type.sign,
          signal_type.unpacked_dims);
      (void)node_id;
    } else if (symbol.direction == slang::ast::ArgumentDirection::Out) {
      NodeId node_id = builder_.create_module_output(
          current_module_id(), std::string(symbol.name), signal_type.width, signal_type.sign,
          std::string(symbol.name), TigBuilder::kInvalidNodeId, 0, signal_type.unpacked_dims);
      (void)node_id;
    } else {
      throw std::logic_error("Unknown port direction");
    }
  }

  void handle(const slang::ast::InstanceBodySymbol &symbol) {
    const auto &definition = symbol.getDefinition();
    if (definition.definitionKind != slang::ast::DefinitionKind::Module) {
      throw std::logic_error(std::string("Unhandled definition kind: ") +
                             definition_kind_to_string(definition.definitionKind));
    }

    if (module_ids_.contains(&symbol)) {
      return;
    }

    ModuleId module_id = builder_.create_module(std::string(definition.name));
    module_ids_[&symbol] = module_id;

    module_stack_.push_back(module_id);
    this->visitDefault(symbol);
    // TODO: sanitize multiple assignment on packed/unpacked variables
    builder_.insert_ffs(module_id);
    builder_.wire_connections(module_id);
    module_stack_.pop_back();
  }

  void handle(const slang::ast::InstanceSymbol &symbol) {
    const std::string suffix = suffix_;
    suffix_.clear();
    this->visitDefault(symbol);
    suffix_ = suffix;

    if (current_module_id() != kInvalidModuleId) {
      const ModuleId module_id = current_module_id();

      const auto &body = symbol.getCanonicalBody() ? *symbol.getCanonicalBody() : symbol.body;
      const auto it = module_ids_.find(&body);
      if (it == module_ids_.end()) {
        throw std::logic_error("Instance references an unknown module body");
      }
      const ModuleId instance_module_id = it->second;

      const NodeId node_id = builder_.create_instance(module_id, std::string(symbol.name) + suffix_,
                                                      instance_module_id);

      for (const auto *conn : symbol.getPortConnections()) {
        if (!conn) {
          throw std::logic_error("Instance contains a null port connection");
        }
        const auto &port_symbol = conn->port;
        if (port_symbol.kind != slang::ast::SymbolKind::Port) {
          throw std::logic_error("Instance connection does not reference a port symbol");
        }
        const auto &port = port_symbol.as<slang::ast::PortSymbol>();
        const slang::ast::Expression *expr = conn->getExpression();
        if (port.direction == slang::ast::ArgumentDirection::In) {
          if (!expr) {
            std::cerr << "warning: leaving unconnected input port: " << symbol.name << "."
                      << port.name << "\n";
            builder_.add_node_input(module_id, node_id, TigBuilder::kInvalidNodeId);
            // TODO: think of a better way of handling this
            continue;
          }
          if (expr->kind != slang::ast::ExpressionKind::NamedValue) {
            const SignalType signal_type = get_signal_type(*expr->type);
            const std::string temporary_name = builder_.create_temporary_signal(
                module_id, signal_type.width, signal_type.sign, signal_type.unpacked_dims);
            const NodeId input_id = create_expr_node(*expr, temporary_name);
            builder_.add_node_input(module_id, node_id, input_id);
          } else {
            SignalWidth width;
            bool sign;
            get_width_sign(*expr->type, width, sign);
            builder_.add_node_input_spec(
                module_id, node_id, extract_named_value(*expr, context_.special_symbols),
                width, sign);
          }
        } else if (port.direction == slang::ast::ArgumentDirection::Out) {
          if (!expr) {
            SignalWidth width;
            bool sign;
            get_width_sign(port.getType(), width, sign);
            builder_.add_node_output(module_id, node_id, "", width, sign);
            continue;
          }
          if (expr->kind != slang::ast::ExpressionKind::Assignment) {
            throw std::logic_error("Output port connection is not an assignment expression");
          }
          const auto &assign = expr->as<slang::ast::AssignmentExpression>();
          const auto &lhs = assign.left();
          SignalWidth rhs_width;
          bool rhs_sign;
          get_width_sign(port.getType(), rhs_width, rhs_sign);
          NodeId output_node_id = node_id;
          ExprId output_expr_id = kInvalidExprId;
          if (assign.right().kind != slang::ast::ExpressionKind::EmptyArgument) {
            if (assign.right().kind != slang::ast::ExpressionKind::Conversion ||
                assign.right().as<slang::ast::ConversionExpression>().operand().kind !=
                    slang::ast::ExpressionKind::EmptyArgument) {
              throw std::logic_error("Unsupported output port conversion expression");
            }
            const std::string temporary_name =
                builder_.create_temporary_signal(module_id, rhs_width, rhs_sign);
            const PortIndex port_idx =
                builder_.add_node_output(module_id, node_id, temporary_name, rhs_width, rhs_sign);
            output_node_id = builder_.create_operation(module_id);
            ExprBuilder expr_builder(builder_.get_expr_graph(module_id, output_node_id));
            const ExprId input_id =
                expr_builder.find_or_create_input(temporary_name, rhs_width, rhs_sign);
            output_expr_id = expr_builder.create_convert(input_id, expr_width(assign.right()),
                                                         expr_sign(assign.right()));
            rhs_width = expr_builder.get_width(output_expr_id);
            rhs_sign = expr_sign(assign.right());
            builder_.add_node_input(module_id, output_node_id, node_id, port_idx);
          }
          if (lhs.kind == slang::ast::ExpressionKind::NamedValue) {
            const std::string output_name = extract_named_value(lhs, context_.special_symbols);
            if (output_expr_id == kInvalidExprId) {
              builder_.add_node_output(module_id, output_node_id, output_name, rhs_width, rhs_sign);
            } else {
              builder_.add_node_output_expr(module_id, output_node_id, output_name, output_expr_id,
                                            true);
            }
          } else {
            const SignalType signal_type = get_signal_type(port.getType());
            const std::string temporary_name = builder_.create_temporary_signal(
                module_id, signal_type.width, signal_type.sign, signal_type.unpacked_dims);
            PortIndex port_idx;
            if (output_expr_id == kInvalidExprId) {
              port_idx = builder_.add_node_output(module_id, output_node_id, temporary_name,
                                                  rhs_width, rhs_sign);
            } else {
              port_idx = builder_.add_node_output_expr(module_id, output_node_id, temporary_name,
                                                       output_expr_id, true);
            }
            const NodeId op_id = builder_.create_operation(module_id);
            ExprBuilder expr_builder(builder_.get_expr_graph(module_id, op_id));
            ExprId rhs_id = expr_builder.find_or_create_input(temporary_name, rhs_width, rhs_sign);
            std::unordered_map<std::string, ExprId> to_store;
            lower_lhs_assignment(lhs, rhs_id, rhs_width, expr_builder, context_, nullptr,
                                 [&](const std::string &output_name, ExprId expr_id) {
                                   expr_builder.update_value(output_name, expr_id);
                                   to_store[output_name] = expr_id;
                                 });
            builder_.add_node_input(module_id, op_id, node_id, port_idx);
            for (const auto &kv : to_store) {
              builder_.add_node_output_expr(module_id, op_id, kv.first, kv.second, true);
            }
          }
        } else {
          throw std::logic_error("Unsupported instance port direction");
        }
      }

      builder_.finalize_node_input(module_id, node_id);
    }
  }

  void handle(const slang::ast::PrimitiveInstanceSymbol &symbol) {
    const std::string_view primitive_name = symbol.primitiveType.name;
    const auto ports = symbol.getPortConnections();
    const bool multi_output_primitive = primitive_name == "buf" || primitive_name == "not";
    const bool multi_input_primitive = primitive_name == "and" || primitive_name == "nand" ||
                                       primitive_name == "or" || primitive_name == "nor" ||
                                       primitive_name == "xor" || primitive_name == "xnor";
    if (!multi_output_primitive && !multi_input_primitive) {
      throw std::logic_error("Unsupported primitive instance: " + std::string(primitive_name));
    }
    const ModuleId module_id = current_module_id();
    const NodeId node_id = builder_.create_operation(module_id);
    ExprBuilder expr_builder(builder_.get_expr_graph(module_id, node_id));
    std::vector<const slang::ast::Expression *> outputs;
    std::vector<ExprId> inputs;
    if (multi_output_primitive) {
      for (size_t i = 0; i + 1 < ports.size(); ++i) {
        outputs.push_back(ports[i]);
      }
      inputs.push_back(build_expr(*ports.back(), expr_builder, context_));
    } else {
      outputs.push_back(ports.front());
      for (size_t i = 1; i < ports.size(); ++i) {
        inputs.push_back(build_expr(*ports[i], expr_builder, context_));
      }
    }
    ExprId rhs_id = kInvalidExprId;
    if (primitive_name == "buf") {
      rhs_id = inputs.front();
    } else if (primitive_name == "not") {
      rhs_id = expr_builder.create_bitwise_not(inputs.front());
    } else if (primitive_name == "and" || primitive_name == "nand") {
      rhs_id = expr_builder.create_and(std::move(inputs));
      if (primitive_name == "nand") {
        rhs_id = expr_builder.create_bitwise_not(rhs_id);
      }
    } else if (primitive_name == "or" || primitive_name == "nor") {
      rhs_id = expr_builder.create_or(std::move(inputs));
      if (primitive_name == "nor") {
        rhs_id = expr_builder.create_bitwise_not(rhs_id);
      }
    } else if (primitive_name == "xor" || primitive_name == "xnor") {
      rhs_id = expr_builder.create_xor(std::move(inputs));
      if (primitive_name == "xnor") {
        rhs_id = expr_builder.create_bitwise_not(rhs_id);
      }
    }
    assert(rhs_id != kInvalidExprId);
    const SignalWidth rhs_width = expr_builder.get_width(rhs_id);
    std::unordered_map<std::string, ExprId> to_store;
    for (const auto *output : outputs) {
      assert(output->kind == slang::ast::ExpressionKind::Assignment);
      const auto &assign = output->as<slang::ast::AssignmentExpression>();
      lower_lhs_assignment(assign.left(), rhs_id, rhs_width, expr_builder, context_, nullptr,
                           [&](const std::string &output_name, ExprId expr_id) {
                             expr_builder.update_value(output_name, expr_id);
                             to_store[output_name] = expr_id;
                           });
    }
    expr_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
      builder_.add_node_input_spec(module_id, node_id, name, width, sign);
    });
    for (const auto &kv : to_store) {
      builder_.add_node_output_expr(module_id, node_id, kv.first, kv.second, true);
    }
  }

  void handle(const slang::ast::ContinuousAssignSymbol &symbol) {
    const auto &assign = symbol.getAssignment();
    assert(assign.kind == slang::ast::ExpressionKind::Assignment);
    const auto &assign_expr = assign.as<slang::ast::AssignmentExpression>();
    const ModuleId module_id = current_module_id();
    const NodeId node_id = builder_.create_operation(module_id);
    ExprBuilder expr_builder(builder_.get_expr_graph(module_id, node_id));
    ExprId rhs_id = build_expr(assign_expr.right(), expr_builder, context_);
    const SignalWidth rhs_width = expr_builder.get_width(rhs_id);
    std::unordered_map<std::string, ExprId> to_store;
    lower_lhs_assignment(assign_expr.left(), rhs_id, rhs_width, expr_builder, context_, nullptr,
                         [&](const std::string &output_name, ExprId expr_id) {
                           expr_builder.update_value(output_name, expr_id);
                           to_store[output_name] = expr_id;
                         });
    expr_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
      builder_.add_node_input_spec(module_id, node_id, name, width, sign);
    });
    for (const auto &kv : to_store) {
      builder_.add_node_output_expr(module_id, node_id, kv.first, kv.second, true);
    }
  }

  void handle(const slang::ast::RootSymbol &symbol) { this->visitDefault(symbol); }

  void handle(const slang::ast::CompilationUnitSymbol &symbol) {
    // TODO: reject or lower compilation-unit variables before recursively visiting the scope.
    this->visitDefault(symbol);
  }

  void handle(const slang::ast::VariableSymbol &symbol) { create_signal(symbol); }

  void handle(const slang::ast::NetSymbol &symbol) {
    const std::string variable_name = create_signal(symbol);
    const auto *init = symbol.getInitializer();
    if (!init) {
      return;
    }
    const ModuleId module_id = current_module_id();
    const NodeId node_id = builder_.create_operation(module_id);
    ExprBuilder expr_builder(builder_.get_expr_graph(module_id, node_id));
    ExprId rhs_id = build_expr(*init, expr_builder, context_);
    expr_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
      builder_.add_node_input_spec(module_id, node_id, name, width, sign);
    });
    builder_.add_node_output_expr(module_id, node_id, variable_name, rhs_id, true);
  }

  void handle(const slang::ast::ProceduralBlockSymbol &symbol) {
    if (symbol.procedureKind == slang::ast::ProceduralBlockKind::Initial ||
        symbol.procedureKind == slang::ast::ProceduralBlockKind::Final) {
      std::cerr << "warning: ignoring procedural block: "
                << slang::ast::SemanticFacts::getProcedureKindStr(symbol.procedureKind) << '\n';
      return;
    }
    const ModuleId module_id = current_module_id();
    const NodeId node_id = builder_.create_operation(module_id);
    StmtBuilder stmt_builder(builder_.get_expr_graph(module_id, node_id));
    switch (symbol.procedureKind) {
    case slang::ast::ProceduralBlockKind::AlwaysComb:
      stmt_builder.set_comb();
      break;
    case slang::ast::ProceduralBlockKind::AlwaysLatch:
      stmt_builder.set_latch();
      break;
    case slang::ast::ProceduralBlockKind::AlwaysFF:
      stmt_builder.set_ff();
      break;
    case slang::ast::ProceduralBlockKind::Always:
      break; // undecided
    default:
      throw std::logic_error("Unknown procedural block kind");
    }
    const slang::ast::Statement &stmt = symbol.getBody();
    lower_statement(stmt, stmt_builder, context_, pragmas_);
    stmt_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
      builder_.add_node_input_spec(module_id, node_id, name, width, sign);
    });
    // TODO: skip integers to be assigned (what else should we skip?)
    if (!stmt_builder.is_ff()) {
      // TODO: latch inference is deferred
      stmt_builder.for_each_output([&](const std::string &name, ExprId expr_id) {
        builder_.add_node_output_expr(module_id, node_id, name, expr_id, stmt_builder.is_comb());
      });
      return;
    }
    std::vector<std::pair<std::string, ExprId>> outputs;
    stmt_builder.for_each_output(
        [&](const std::string &name, ExprId expr_id) { outputs.emplace_back(name, expr_id); });
    std::string clk_name, rst_name;
    SignalWidth clk_width, rst_width;
    bool clk_sign, rst_sign;
    EdgeKind clk_edge, rst_edge;
    stmt_builder.get_timing_spec(outputs, clk_name, clk_width, clk_sign, clk_edge, rst_name,
                                 rst_width, rst_sign, rst_edge);
    for (const auto &kv : outputs) {
      const PortIndex port_idx = builder_.add_node_output_expr(module_id, node_id, kv.first,
                                                               kv.second, stmt_builder.is_comb());
      builder_.record_ff(module_id, kv.first, {clk_name, clk_width, clk_sign}, clk_edge,
                         {rst_name, rst_width, rst_sign}, rst_edge, node_id, port_idx);
    }
  }

  void handle(const slang::ast::SubroutineSymbol &symbol) {
    if (symbol.subroutineKind != slang::ast::SubroutineKind::Function) {
      std::cerr << "warning: ignoring task in synthesis lowering: " << symbol.name << "\n";
      return;
    }
    // TODO: it is better to remove dependency on tig structure; use builder api to create a
    // subroutine
    Tig::Subroutine subr;
    subr.name = std::string(symbol.name);
    for (const auto *arg : symbol.getArguments()) {
      // TODO: handle packed/unpacked array
      if (arg->direction != slang::ast::ArgumentDirection::In) {
        throw std::logic_error("Only input formals are supported in function lowering: " +
                               std::string(symbol.name) + "." + std::string(arg->name));
      }
      const auto &type = arg->getType();
      subr.inputs.emplace_back(
          Tig::Subroutine::Port{std::string(arg->name), type.getBitstreamWidth(), type.isSigned()});
    }
    StmtBuilder stmt_builder(subr.expr_graph);
    const auto &return_type = symbol.getReturnType();
    const SignalWidth return_width = return_type.getBitstreamWidth();
    const std::string return_unknown(return_width, 'x');
    stmt_builder.get_expr_builder().update_value(
        std::string(symbol.name), stmt_builder.get_expr_builder().find_or_create_const(
                                      std::to_string(return_width) + "'b" + return_unknown,
                                      return_width, return_type.isSigned()));
    lower_statement(symbol.getBody(), stmt_builder, context_, pragmas_);
    const ExprId ret = stmt_builder.get_expr_builder().get_current_value(symbol.name);
    if (ret == kInvalidExprId) {
      throw std::logic_error("Function has no return assignment: " + std::string(symbol.name));
    }
    subr.expr_root = ret;
    builder_.add_subroutine(context_.get_or_create_subroutine_id(symbol), std::move(subr));
  }

  void handle(const slang::ast::StatementBlockSymbol &symbol) {
    std::string suffix = suffix_;
    if (suffix_.empty()) {
      suffix_ = "_abys";
    }
    // TODO: maybe generate a random signature when symbol.name.empty()
    suffix_ += "_" + std::string(symbol.name);
    this->visitDefault(symbol);
    suffix_ = suffix;
  }

  void handle(const slang::ast::GenerateBlockSymbol &symbol) {
    if (symbol.isUninstantiated) {
      return;
    }
    std::string frag = "_";
    frag += symbol.getExternalName();
    if (symbol.arrayIndex) {
      auto idx = symbol.arrayIndex->as<int64_t>();
      if (idx) {
        frag += "_" + std::to_string(*idx);
      }
    }
    std::string suffix = suffix_;
    if (suffix_.empty()) {
      suffix_ = "_abys"; // TODO: add prefix/suffix management system for internal signals
    }
    suffix_ += frag;
    this->visitDefault(symbol);
    suffix_ = suffix;
  }

  void handle(const slang::ast::GenerateBlockArraySymbol &symbol) { this->visitDefault(symbol); }

  void handle(const slang::ast::TransparentMemberSymbol &symbol) { this->visitDefault(symbol); }
};

void lower_slang_ast_to_ir(const slang::ast::RootSymbol &root, TigBuilder &builder,
                           const PragmaMap &pragmas) {
  SlangLoweringVisitor visitor(builder, pragmas);
  root.visit(visitor);
  builder.flatten_calls();
}

void lower_slang_ast_to_ir(const slang::ast::RootSymbol &root, TigBuilder &builder) {
  const PragmaMap pragmas;
  lower_slang_ast_to_ir(root, builder, pragmas);
}

} // namespace abys::frontend
