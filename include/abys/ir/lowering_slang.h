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
#include "slang/ast/expressions/OperatorExpressions.h"
#include "slang/ast/expressions/MiscExpressions.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/symbols/MemberSymbols.h"
#include "slang/ast/symbols/PortSymbols.h"
#include "slang/ast/types/Type.h"
#include "slang/driver/Driver.h"

#include "abys/ir/expr_builder.h"
#include "abys/ir/stmt_builder.h"
#include "abys/ir/tig_builder.h"

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

  std::string extract_named_value(const slang::ast::Expression &expr) {
    assert(expr.kind == slang::ast::ExpressionKind::NamedValue);
    const auto &named = expr.as<slang::ast::NamedValueExpression>();
    return std::string(named.symbol.name);
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

    void handle(const slang::ast::IntegerLiteral &expr) {
      const slang::SVInt v = expr.getValue();
      ExprId id = builder_.create_const(v.toString(slang::LiteralBase::Binary), expr_width(expr), expr_sign(expr));
      expr_stack_.push_back(id);
    }

    void handle(const slang::ast::UnbasedUnsizedIntegerLiteral &expr) {
      const slang::SVInt v = expr.getValue();
      ExprId id = builder_.create_const(v.toString(slang::LiteralBase::Binary), expr_width(expr), expr_sign(expr));
      expr_stack_.push_back(id);
    }
    
    void handle(const slang::ast::BinaryExpression &expr) {
      this->visitDefault(expr);
      assert(expr_stack_.size() >= 2);
      ExprId rhs = expr_stack_.back();
      expr_stack_.pop_back();
      ExprId lhs = expr_stack_.back();
      expr_stack_.pop_back();
      ExprNode::Op op = ExprNode::Op::kConst;
      switch (expr.op) {
      case slang::ast::BinaryOperator::BinaryAnd:
        op = ExprNode::Op::kAnd;
        break;
      case slang::ast::BinaryOperator::BinaryOr:
        op = ExprNode::Op::kOr;
        break;
      case slang::ast::BinaryOperator::BinaryXor:
        op = ExprNode::Op::kXor;
        break;
      case slang::ast::BinaryOperator::Add:
        op = ExprNode::Op::kAdd;
        break;
      case slang::ast::BinaryOperator::Subtract:
        op = ExprNode::Op::kSub;
        break;
      case slang::ast::BinaryOperator::Multiply:
        op = ExprNode::Op::kMul;
        break;
      case slang::ast::BinaryOperator::LogicalShiftLeft:
      case slang::ast::BinaryOperator::ArithmeticShiftLeft:
        op = ExprNode::Op::kShl;
        break;
      case slang::ast::BinaryOperator::LogicalShiftRight:
        op = ExprNode::Op::kShr;
        break;
      case slang::ast::BinaryOperator::ArithmeticShiftRight:
        op = ExprNode::Op::kAshr;
        break;
      case slang::ast::BinaryOperator::Equality:
      case slang::ast::BinaryOperator::CaseEquality:
        op = ExprNode::Op::kEq;
        break;
      case slang::ast::BinaryOperator::Inequality:
      case slang::ast::BinaryOperator::CaseInequality:
        op = ExprNode::Op::kNeq;
        break;
      case slang::ast::BinaryOperator::LessThan:
        op = ExprNode::Op::kLt;
        break;
      case slang::ast::BinaryOperator::LessThanEqual:
        op = ExprNode::Op::kLe;
        break;
      case slang::ast::BinaryOperator::GreaterThan:
        op = ExprNode::Op::kGt;
        break;
      case slang::ast::BinaryOperator::GreaterThanEqual:
        op = ExprNode::Op::kGe;
        break;
      default:
        throw std::logic_error(
            std::string("Unhandled binary operator: ")
            + std::string(slang::ast::OpInfo::getText(expr.op)));
      }
      ExprId id = builder_.create_binary(op, lhs, rhs, expr_width(expr), expr_sign(expr));
      expr_stack_.push_back(id);
    }

    ExprId get_root() {
      assert(!expr_stack_.empty());
      assert(expr_stack_.size() == 1);
      return expr_stack_.back();
    }
  };

  ExprId build_expr(const slang::ast::Expression &expr, ExprBuilder &expr_builder) {
    SlangExprLoweringVisitor<ExprBuilder> expr_visitor(expr_builder);
    expr.visit(expr_visitor);
    return expr_visitor.get_root();
  }


  template <typename Builder>
  class SlangStmtLoweringVisitor final
    : public slang::ast::ASTVisitor<SlangStmtLoweringVisitor<Builder>, true, false, false, true> {
  private:
    Builder &builder_;

    std::string extract_lhs_name(const slang::ast::Expression &expr) {
      assert(expr.kind == slang::ast::ExpressionKind::Assignment);
      const auto &assign = expr.as<slang::ast::AssignmentExpression>();
      return extract_named_value(assign.left());
    }
    
 public:
    explicit SlangStmtLoweringVisitor(Builder &builder) : builder_(builder) {}


    template<typename T>
    void handle(const T&) {
      throw std::logic_error(
                             std::string("Unhandled AST node: ") + typeid(T).name()
                             );
    }

    void handle(const slang::ast::ExpressionStatement &stmt) {
      if (stmt.expr.kind != slang::ast::ExpressionKind::Assignment) {
	throw std::logic_error("Non-assignment expression statement is unsupported");
      }
      const auto &assign = stmt.expr.as<slang::ast::AssignmentExpression>();
      assert(!assign.isCompound()); // we need to handle this later
      ExprBuilder expr_builder = builder_.make_expr_builder();
      ExprId expr_id = build_expr(assign.right(), expr_builder);
      std::string output_name = extract_lhs_name(assign);
      bool nonblocking = assign.isNonBlocking();
      if (!nonblocking) {
	builder_.current_values()[output_name] = expr_id;
      }
      builder_.output_names().push_back(output_name);
      builder_.output_nonblocking().push_back(nonblocking);
      builder_.output_ids().push_back(expr_id);
    }
    
    void handle(const slang::ast::ConditionalStatement &stmt) {
      ExprBuilder expr_builder = builder_.make_expr_builder();
      std::vector<ExprId> cond_ids;
      for (const auto &cond : stmt.conditions) {
	ExprId cond_id = build_expr(*cond.expr, expr_builder);
	cond_ids.push_back(cond_id);
      }
      assert(!cond_ids.empty());
      ExprId cond_id = kInvalidExprId;
      if (cond_ids.size() > 1) {
	cond_id = expr_builder.create_nary(ExprNode::Op::kAnd, std::move(cond_ids), 1, false);
      } else {
	cond_id = cond_ids[0];
      }
      builder_.create_context();
      stmt.ifTrue.visit(*this);
      builder_.stack_context();
      builder_.create_context();
      if (stmt.ifFalse) {
	stmt.ifFalse->visit(*this);
      }
      builder_.stack_context();
      builder_.merge_conditional(cond_id);
    }

    void handle(const slang::ast::CaseStatement &stmt) {
      size_t index = builder_.get_context_stack_index();
      ExprBuilder expr_builder = builder_.make_expr_builder();
      ExprId case_id = build_expr(stmt.expr, expr_builder);
      std::vector<ExprId> case_values;
      for (auto &item : stmt.items) {
	std::vector<ExprId> values;
	for (auto *v : item.expressions) {
	  values.push_back(build_expr(*v, expr_builder));
	}
	ExprId value_id = kInvalidExprId;
	if (values.size() > 1) {
	  value_id = expr_builder.create_list(std::move(values));
	} else {
	  value_id = values[0];
	}
	case_values.push_back(value_id);
	builder_.create_context();
	item.stmt->visit(*this);
	builder_.stack_context();
      }
      builder_.create_context();
      if (stmt.defaultCase) {
	stmt.defaultCase->visit(*this);
      }
      builder_.stack_context();
      builder_.merge_case(case_id, case_values, index);
    }
    
    void handle(const slang::ast::StatementList &stmt) {
      this->visitDefault(stmt);
    }
    
    void handle(const slang::ast::BlockStatement &stmt) {
      builder_.create_context();
      this->visitDefault(stmt);
      builder_.merge_context();
    }

    void handle(const slang::ast::SignalEventControl &ev) {
      ExprBuilder expr_builder = builder_.make_expr_builder();
      ExprId expr_id = build_expr(ev.expr, expr_builder);
      ExprId iff_id = ev.iffCondition ? build_expr(*ev.iffCondition, expr_builder) : kInvalidExprId;
      const bool posedge = ev.edge == slang::ast::EdgeKind::PosEdge;
      const bool negedge = ev.edge == slang::ast::EdgeKind::NegEdge;
      const bool both = ev.edge == slang::ast::EdgeKind::BothEdges;
      builder_.add_timing(expr_id, iff_id, posedge || both, negedge || both);
    }
    
    void handle(const slang::ast::EventListControl &ev) {
      this->visitDefault(ev);
    }
    
    void handle(const slang::ast::TimedStatement &stmt) {
      if(!builder_.is_root_context()) {
        throw std::logic_error("TimedStatement in a non-root context");
      }
      if(!builder_.is_ff() && !builder_.is_undecided()) {
        throw std::logic_error("TimedStatement in always_comb or always_latch");
      }
      if(builder_.has_timing()) {
	throw std::logic_error("Nested TimedStatement");
      }
      this->visitDefault(stmt);
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

    NodeId create_expr_node(const slang::ast::Expression &expr, std::string output_name = "") {
      const ModuleId module_id = current_module_id();
      const NodeId node_id = builder_.create_operation(module_id);
      ExprBuilder expr_builder(builder_.get_expr_nodes_ref(module_id, node_id));
      ExprId expr_id = build_expr(expr, expr_builder);
      expr_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
        builder_.add_node_input_spec(module_id, node_id, name, width, sign);
      });
      builder_.add_node_output_expr(module_id, node_id, std::move(output_name), expr_id);
      return node_id;
    }

  public:
    explicit SlangLoweringVisitor(Builder &builder) : builder_(builder) {}

  private:
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

    void handle(const slang::ast::ContinuousAssignSymbol &symbol) {
      const auto &assign = symbol.getAssignment();
      assert(assign.kind == slang::ast::ExpressionKind::Assignment);
      const auto &assign_expr = assign.as<slang::ast::AssignmentExpression>();
      const std::string output_name = extract_named_value(assign_expr.left());
      (void)create_expr_node(assign_expr.right(), output_name);
    }
    
    void handle(const slang::ast::RootSymbol &symbol) {
      this->visitDefault(symbol);
    }

    void handle(const slang::ast::CompilationUnitSymbol &symbol) {
      this->visitDefault(symbol);
    }

    void handle(const slang::ast::VariableSymbol &symbol) {
      // TODO: implement this for debugging (mismatch, unused, or nondeclared)
      (void)symbol;
    }

    void handle(const slang::ast::ProceduralBlockSymbol &symbol) {
      const ModuleId module_id = current_module_id();
      const NodeId node_id = builder_.create_operation(module_id);
      StmtBuilder stmt_builder(builder_.get_expr_nodes_ref(module_id, node_id));
      switch (symbol.procedureKind) {
      case slang::ast::ProceduralBlockKind::AlwaysComb: stmt_builder.set_comb(); break;
      case slang::ast::ProceduralBlockKind::AlwaysLatch: stmt_builder.set_latch(); break;
      case slang::ast::ProceduralBlockKind::AlwaysFF: stmt_builder.set_ff(); break;
      case slang::ast::ProceduralBlockKind::Always: stmt_builder.set_comb_or_latch(); break;
      default: throw std::logic_error("Unknown procedural block kind");
      }
      SlangStmtLoweringVisitor<StmtBuilder> stmt_visitor(stmt_builder);
      const slang::ast::Statement& stmt = symbol.getBody();
      stmt.visit(stmt_visitor);
      stmt_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
        builder_.add_node_input_spec(module_id, node_id, name, width, sign);
      });
      stmt_builder.for_each_output([&](const std::string &name, ExprId expr_id) {
	// TODO: think about how to handle sequential stuff
	builder_.add_node_output_expr(module_id, node_id, name, expr_id);
      });
    }
    
  };
  
  
  template <typename Builder>
    void lower_slang_ast_to_ir(const slang::ast::RootSymbol &root, Builder &builder) {
    SlangLoweringVisitor<Builder> visitor(builder);
    root.visit(visitor);
  }
}
