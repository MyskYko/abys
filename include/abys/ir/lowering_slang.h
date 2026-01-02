#pragma once

#include <cassert>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "slang/ast/Compilation.h"
#include "slang/ast/SemanticFacts.h"
#include "slang/ast/ASTVisitor.h"
#include "slang/ast/expressions/AssignmentExpressions.h"
#include "slang/ast/expressions/ConversionExpression.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/symbols/PortSymbols.h"
#include "slang/ast/types/Type.h"
#include "slang/driver/Driver.h"

#include "abys/ir/tig_builder.h"
#include "abys/ir/expr_builder.h"

namespace abys::ir {

  static inline const char* definitionKindToString(slang::ast::DefinitionKind kind) {
    switch (kind) {
    case slang::ast::DefinitionKind::Module:
      return "Module";
    case slang::ast::DefinitionKind::Interface:
      return "Interface";
    case slang::ast::DefinitionKind::Program:
      return "Program";
    default:
      return "Unknown";
    }
  }
  
  static inline abys::ir::SignalWidth expr_width(const slang::ast::Expression &expr) {
    return expr.type->getBitstreamWidth();
  }
  
  static inline bool expr_sign(const slang::ast::Expression &expr) {
    return expr.type->isSigned();
  }

  template <typename Builder>
    class SlangExprLoweringVisitor final
    : public slang::ast::ASTVisitor<SlangExprLoweringVisitor<Builder>, false, true, false, true> {
  private:
        Builder &builder_;
    std::vector<ExprId> expr_stack_;

 public:
    explicit SlangExprLoweringVisitor(Builder &builder) : builder_(builder) {}

    template<typename T>
    void handle(const T&) {
      throw std::logic_error(
                             std::string("Unhandled AST node: ") + typeid(T).name()
                             );
    }

    void handle(const slang::ast::ConversionExpression &expr) {
      this->visitDefault(expr);
      ExprId operand = expr_stack_.back();
      expr_stack_.pop_back();
      ExprId id = builder_.create_convert(operand, expr_width(expr), expr_sign(expr));
      expr_stack_.push_back(id);
    }

    void handle(const slang::ast::NamedValueExpression &expr) {
      ExprId id = builder_.find_or_create_input(std::string(expr.symbol.name), expr_width(expr),
                                                expr_sign(expr));
      expr_stack_.push_back(id);
    }

    ExprId get_root() {
      assert(!expr_stack_.empty());
      assert(expr_stack_.size() == 1);
      return expr_stack_.back();
    }
  };
    
  template <typename Builder>
    class SlangLoweringVisitor final
    : public slang::ast::ASTVisitor<SlangLoweringVisitor<Builder>, false, false, false, true> {
  private:

    using ModuleId = typename Builder::ModuleId;
    using NodeId = typename Builder::NodeId;
    static constexpr ModuleId kInvalidModuleId = Builder::kInvalidModuleId;
    static constexpr NodeId kInvalidNodeId = Builder::kInvalidNodeId;
    using Signal = typename Builder::Signal;
    using SignalSpec = typename Builder::SignalSpec;

    Builder &builder_;

    std::vector<ModuleId> module_stack_;
    std::unordered_map<const slang::ast::InstanceBodySymbol *, ModuleId> module_ids_;

    ModuleId current_module_id() const {
      if (module_stack_.empty()) {
	return kInvalidModuleId;
      }
      return module_stack_.back();
    }

    ExprId build_expr(const slang::ast::Expression &expr, ExprBuilder &expr_builder) {
      SlangExprLoweringVisitor<ExprBuilder> expr_visitor(expr_builder);
      expr.visit(expr_visitor);
      return expr_visitor.get_root();
    }

    NodeId create_expr_node(const slang::ast::Expression &expr, std::string output_name = "") {
      const ModuleId module_id = current_module_id();
      const NodeId node_id = builder_.create_operation(module_id);
      ExprBuilder expr_builder(builder_.get_expr_nodes_ref(module_id, node_id));
      ExprId expr_id = build_expr(expr, expr_builder);
      expr_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
        builder_.add_node_input_spec(module_id, node_id, name, width, sign);
      });
      builder_.add_node_output(module_id, node_id, std::move(output_name),
                               expr_builder.get_width(expr_id), expr_builder.get_sign(expr_id));
      return node_id;
    }

  public:
    explicit SlangLoweringVisitor(Builder &builder) : builder_(builder) {}

  private:
    std::string extract_named_value(const slang::ast::Expression &expr) {
      assert(expr.kind == slang::ast::ExpressionKind::NamedValue);
      const auto &named = expr.as<slang::ast::NamedValueExpression>();
      return std::string(named.symbol.name);
    }

    std::string extract_output_named_value(const slang::ast::Expression &expr) {
      assert(expr.kind == slang::ast::ExpressionKind::Assignment);
      const auto &assign = expr.as<slang::ast::AssignmentExpression>();
      assert(assign.right().kind == slang::ast::ExpressionKind::EmptyArgument);
      return extract_named_value(assign.left());
    }

    abys::ir::SignalWidth port_width(const slang::ast::PortSymbol &port) {
      return port.getType().getBitstreamWidth();
    }

    bool port_sign(const slang::ast::PortSymbol &port) {
      return port.getType().isSigned();
    }

  public:

    template<typename T>
      void handle(const T&) {
      throw std::logic_error(
			     std::string("Unhandled AST node: ") + typeid(T).name()
			     );
    }
    
    void handle(const slang::ast::PortSymbol &symbol) {
      this->visitDefault(symbol);
      if (symbol.direction == slang::ast::ArgumentDirection::InOut) {
        throw std::logic_error("InOut ports are not supported");
      }
      if (symbol.direction == slang::ast::ArgumentDirection::Ref) {
        throw std::logic_error("Ref ports are not supported");
      }
      if(symbol.direction == slang::ast::ArgumentDirection::In) {
	NodeId node_id = builder_.create_module_input(current_module_id(), std::string(symbol.name),
                                                     port_width(symbol), port_sign(symbol));
        (void)node_id;
      } else if(symbol.direction == slang::ast::ArgumentDirection::Out) {
	NodeId node_id = builder_.create_module_output(current_module_id(), std::string(symbol.name),
                                                      port_width(symbol), port_sign(symbol),
						      std::string(symbol.name), port_width(symbol),
                                                      port_sign(symbol));
        (void)node_id;
      } else {
	throw std::logic_error("Unknown port direction");
      }
    }

    void handle(const slang::ast::InstanceBodySymbol &symbol) {
      const auto &definition = symbol.getDefinition();
      if (definition.definitionKind != slang::ast::DefinitionKind::Module) {
	throw std::logic_error(
			       std::string("Unhandled definition kind: ")
			       + definitionKindToString(definition.definitionKind)
			       );
      }
      
      if(module_ids_.contains(&symbol)) {
	return;
      }
      
      ModuleId module_id = builder_.create_module(std::string(definition.name));
      module_ids_[&symbol] = module_id;

      module_stack_.push_back(module_id);
      this->visitDefault(symbol);
      builder_.wire_connections(module_id);
      module_stack_.pop_back();
    }

    void handle(const slang::ast::InstanceSymbol &symbol) {
      this->visitDefault(symbol);

      if(current_module_id() != kInvalidModuleId) {
	const ModuleId module_id = current_module_id();
	
	const auto &body = symbol.getCanonicalBody() ? *symbol.getCanonicalBody() : symbol.body;
	auto it = module_ids_.find(&body);
	assert(it != module_ids_.end());
	const ModuleId instance_module_id = it->second;

	const NodeId node_id = builder_.create_instance(module_id, std::string(symbol.name), instance_module_id);

	for (const auto *conn : symbol.getPortConnections()) {
	  assert(conn);
	  const auto &port_symbol = conn->port;
	  assert(port_symbol.kind == slang::ast::SymbolKind::Port);
	  const auto &port = port_symbol.as<slang::ast::PortSymbol>();
	  assert(port.direction != slang::ast::ArgumentDirection::InOut);
	  assert(port.direction != slang::ast::ArgumentDirection::Ref);
	  const slang::ast::Expression *expr = conn->getExpression();
	  assert(expr);
	  if (port.direction == slang::ast::ArgumentDirection::In) {
	    if (expr->kind == slang::ast::ExpressionKind::Conversion) {
	      const NodeId input_id = create_expr_node(*expr);
	      builder_.add_node_input(module_id, node_id, input_id);
	    } else {
	      builder_.add_node_input_spec(module_id, node_id, extract_named_value(*expr),
                                           expr_width(*expr), expr_sign(*expr));
	    }
	  } else if (port.direction == slang::ast::ArgumentDirection::Out) {
	    builder_.add_node_output(module_id, node_id, extract_output_named_value(*expr),
                                     port_width(port), port_sign(port));
	  } else {
	    assert(false);
	  }
	}
	
	builder_.finalize_node_input(module_id, node_id);
      }
    }
    
    void handle(const slang::ast::RootSymbol &symbol) {
      this->visitDefault(symbol);
    }
  };
  
  
  template <typename Builder>
    void lower_slang_ast_to_ir(const slang::ast::RootSymbol &root, Builder &builder) {
    SlangLoweringVisitor<Builder> visitor(builder);
    root.visit(visitor);
  }
}
