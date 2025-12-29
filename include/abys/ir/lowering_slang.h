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

namespace abys::ir {

  inline const char* definitionKindToString(slang::ast::DefinitionKind kind) {
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
  
  
  template <typename Builder>
    class SlangLoweringVisitor final
    : public slang::ast::ASTVisitor<SlangLoweringVisitor<Builder>, false, false, false, true> {
  private:

    using ModuleId = typename Builder::ModuleId;
    using NodeId = typename Builder::NodeId;
    using SignalWidth = typename Builder::SignalWidth;
    static constexpr ModuleId kInvalidModuleId = Builder::kInvalidModuleId;
    static constexpr NodeId kInvalidNodeId = Builder::kInvalidNodeId;
    using Signal = typename Builder::Signal;
    using SignalSpec = typename Builder::SignalSpec;

    struct ModuleContext {
      ModuleId module_id;
      std::unordered_map<NodeId, std::vector<SignalSpec>> node_inputs;
    };

    Builder &builder_;

    std::vector<ModuleContext> module_stack_;
    std::unordered_map<const slang::ast::InstanceBodySymbol *, ModuleId> module_ids_;

    ModuleId current_module_id() const {
      if (module_stack_.empty()) {
	return kInvalidModuleId;
      }
      return module_stack_.back().module_id;
    }

    void record_input(NodeId node_id, std::string name, SignalWidth width, bool sign) {
      if (module_stack_.empty()) {
	throw std::logic_error("module stack is empty");
      }
      module_stack_.back().node_inputs[node_id].emplace_back(name, width, sign);
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

    SignalWidth port_width(const slang::ast::PortSymbol &port) {
      return port.getType().getBitstreamWidth();
    }

    bool port_sign(const slang::ast::PortSymbol &port) {
      return port.getType().isSigned();
    }

    SignalWidth expr_width(const slang::ast::Expression &expr) {
      return expr.type->getBitstreamWidth();
    }

    bool expr_sign(const slang::ast::Expression &expr) {
      return expr.type->isSigned();
    }

    void prepare_input(const slang::ast::Expression &expr, std::vector<Signal> &node_inputs,
                       std::vector<SignalSpec> &node_input_specs) {
      if (expr.kind == slang::ast::ExpressionKind::Conversion) {
	NodeId node_id = builder_.create_conversion_node(
            current_module_id(), "", expr_width(expr), expr_sign(expr), kInvalidNodeId);
	const auto &conv = expr.as<slang::ast::ConversionExpression>();
	const std::string name = extract_named_value(conv.operand());
	const uint64_t width = conv.operand().type->getBitstreamWidth();
	const bool sign = conv.operand().type->isSigned();
	record_input(node_id, name, width, sign);
	node_inputs.emplace_back(node_id, 0);
	node_input_specs.emplace_back("", 0, false);
      } else {
	node_inputs.emplace_back(kInvalidNodeId, 0);
	node_input_specs.emplace_back(extract_named_value(expr), expr_width(expr), expr_sign(expr));
      }
    }

    void wire_connections() {
      ModuleId module_id = current_module_id();
      for (const auto &entry : module_stack_.back().node_inputs) {
	const NodeId node_id = entry.first;
	for (size_t i = 0; i < entry.second.size(); i++) {
	  const std::string &name = entry.second[i].name;
	  if (!name.empty()) {
	    Signal input = builder_.find_signal(module_id, name);
            assert(input.node_id != kInvalidNodeId);
            const auto spec = builder_.get_signal_spec(module_id, input);
            assert(entry.second[i].width == spec.width);
            assert(entry.second[i].sign == spec.sign);
	    builder_.set_node_input(module_id, node_id, i, input);
	  } else {
	    Signal input = builder_.get_node_input(module_id, node_id, i);
	    assert(input.node_id != kInvalidNodeId);
	  }
	}
      }
    }

  public:

    template<typename T>
      void handle(const T&) {
      throw std::logic_error(
			     std::string("Unhandled AST node: ") + typeid(T).name()
			     );
    }
    
    void handle(const slang::ast::RootSymbol &symbol) {
      this->visitDefault(symbol);
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

      module_stack_.push_back({module_id, {}});
      this->visitDefault(symbol);
      wire_connections();
      module_stack_.pop_back();
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
                                                      kInvalidNodeId);
	record_input(node_id, std::string(symbol.name), port_width(symbol), port_sign(symbol));
      } else {
	throw std::logic_error("Unknown port direction");
      }
    }

    void handle(const slang::ast::InstanceSymbol &symbol) {
      this->visitDefault(symbol);

      const auto &body = symbol.getCanonicalBody() ? *symbol.getCanonicalBody() : symbol.body;
      auto it = module_ids_.find(&body);
      assert(it != module_ids_.end());
      ModuleId instance_module_id = it->second;

      std::vector<Signal> node_inputs;
      std::vector<SignalSpec> node_outputs;
      std::vector<SignalSpec> node_input_specs;
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
	  prepare_input(*expr, node_inputs, node_input_specs);
	} else if (port.direction == slang::ast::ArgumentDirection::Out) {
	  node_outputs.emplace_back(extract_output_named_value(*expr), port_width(port),
                                    port_sign(port));
	} else {
	  assert(false);
	}
      }

      if(current_module_id() != kInvalidModuleId) {
	NodeId instance_id = builder_.create_instance(current_module_id(), std::string(symbol.name),
                                                      instance_module_id, node_inputs,
                                                      node_outputs);
	for(auto &spec: node_input_specs) {
	  record_input(instance_id, spec.name, spec.width, spec.sign);
	}
      }
    }
  };
  
  
  template <typename Builder>
    void lower_slang_ast_to_ir(const slang::ast::RootSymbol &root, Builder &builder) {
    SlangLoweringVisitor<Builder> visitor(builder);
    root.visit(visitor);
  }
}
