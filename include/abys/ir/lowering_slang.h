#pragma once

#include <cassert>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#include <iostream>

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

  BitIndex extract_constant_index(const slang::ast::Expression &expr) {
    const auto cv = expr.getConstant();
    if (!cv || !*cv || !cv->isInteger()) {
      throw std::logic_error("Expected integer constant");
    }
    const slang::SVInt &sv = cv->integer();
    auto v = sv.as<int64_t>();
    if (!v) {
      throw std::logic_error("SVInt too wide for int64");
    }
    return *v;
  }

  void get_width_sign(const slang::ast::Type &type, SignalWidth &width, bool &sign) {
    if (type.isUnpackedArray()) {
      const auto &ct = type.getCanonicalType();
      if (ct.kind != slang::ast::SymbolKind::FixedSizeUnpackedArrayType) {
        throw std::logic_error("Unsupported dynamic size unpacked array");
      }
      const auto &arr = ct.as<slang::ast::FixedSizeUnpackedArrayType>();
      const auto range = arr.range;
      width = (range.left >= range.right) ? (range.left - range.right + 1) : (range.right - range.left + 1);
      sign = false;
    } else {
      width = type.getBitstreamWidth();
      sign = type.isSigned();
    }
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
      const auto &type = *expr.type;
      SignalWidth width;
      bool sign;
      get_width_sign(type, width, sign);
      ExprId id = builder_.find_or_create_input(std::string(expr.symbol.name), width, sign);      
      expr_stack_.push_back(id);
    }

    void handle(const slang::ast::IntegerLiteral &expr) {
      const slang::SVInt v = expr.getValue();
      ExprId id = builder_.find_or_create_const(v.toString(slang::LiteralBase::Binary), expr_width(expr), expr_sign(expr));
      expr_stack_.push_back(id);
    }

    void handle(const slang::ast::StringLiteral &expr) {
      const auto cv = expr.getConstant();
      if (!cv || !*cv || !cv->isInteger()) {
        throw std::logic_error("String literal did not lower to integer constant");
      }
      const slang::SVInt v = cv->integer();
      ExprId id = builder_.find_or_create_const(v.toString(slang::LiteralBase::Binary), expr_width(expr), expr_sign(expr));
      expr_stack_.push_back(id);
    }

    void handle(const slang::ast::UnbasedUnsizedIntegerLiteral &expr) {
      const slang::SVInt v = expr.getValue();
      ExprId id = builder_.find_or_create_const(v.toString(slang::LiteralBase::Binary), expr_width(expr), expr_sign(expr));
      expr_stack_.push_back(id);
    }

    void handle(const slang::ast::CallExpression &expr) {
      // TODO: it seems some parameters do not get evaluated as a constant, so remembering system call may be necessary as well, then cv stuff may not be needed any longer
      const auto cv = expr.getConstant();
      if (cv && *cv && cv->isInteger()) {
        const slang::SVInt v = cv->integer();
        const ExprId id = builder_.find_or_create_const(v.toString(slang::LiteralBase::Binary), expr_width(expr), expr_sign(expr));
        expr_stack_.push_back(id);
        return;
      }
      if (expr.thisClass() != nullptr) {
        throw std::logic_error("Unsupported class member call: " + std::string(expr.getSubroutineName()));
      }
      if (expr.isSystemCall()) {
        throw std::logic_error("Unsupported system call: " + std::string(expr.getSubroutineName()));
      }
      const size_t index = expr_stack_.size();
      this->visitDefault(expr);
      const size_t n = expr_stack_.size() - index;
      std::vector<ExprId> operands(n);
      for (size_t i = 0; i < n; i++) {
        operands[n - 1 - i] = expr_stack_.back();
        expr_stack_.pop_back();
      }
      std::string name(expr.getSubroutineName());
      const auto *subr_ptr = std::get<const slang::ast::SubroutineSymbol*>(expr.subroutine);
      const ExprId id = builder_.create_call(subr_ptr, std::move(name), std::move(operands), expr_width(expr), expr_sign(expr));
      expr_stack_.push_back(id);
    }
    
    void handle(const slang::ast::ElementSelectExpression &expr) {
      this->visitDefault(expr);
      const ExprId index = expr_stack_.back();
      expr_stack_.pop_back();
      const ExprId data  = expr_stack_.back();
      expr_stack_.pop_back();
      const auto &type = *expr.value().type;
      if (type.isUnpackedArray()) {
        const auto &ct = type.getCanonicalType();
        if (ct.kind != slang::ast::SymbolKind::FixedSizeUnpackedArrayType) {
          throw std::logic_error("Unsupported dynamic size unpacked array");
        }
        const auto &arr = ct.as<slang::ast::FixedSizeUnpackedArrayType>();
        const auto range = arr.range;
        const auto &elem = arr.elementType;
        SignalWidth width;
        bool sign;
        get_width_sign(elem, width, sign);
        ExprId id = builder_.create_array_select(data, index, range.left, range.right, width, sign);
        expr_stack_.push_back(id);
      } else {
        auto range = type.getFixedRange();
        const ExprId id = builder_.create_select(data, index, range.left, range.right);
        expr_stack_.push_back(id);
      } 
    }
    
    void handle(const slang::ast::RangeSelectExpression &expr) {
      expr.value().visit(*this);
      const ExprId data = expr_stack_.back();
      expr_stack_.pop_back();
      const auto &type = *expr.value().type;
      const auto kind = expr.getSelectionKind();
      const auto &left = expr.left();
      const auto &right = expr.right();
      slang::ConstantRange range;
      if (type.isUnpackedArray()) {
        const auto &ct = type.getCanonicalType();
        if (ct.kind != slang::ast::SymbolKind::FixedSizeUnpackedArrayType) {
          throw std::logic_error("Unsupported dynamic size unpacked array");
        }
        const auto &arr = ct.as<slang::ast::FixedSizeUnpackedArrayType>();
        range = arr.range;
      } else {
        range = type.getFixedRange();
      }
      if (kind == slang::ast::RangeSelectionKind::Simple) {
        const BitIndex left_sw = extract_constant_index(left);
        const BitIndex right_sw = extract_constant_index(right);
        expr_stack_.push_back(builder_.create_range(data, left_sw, right_sw, range.left, range.right));
      } else if (kind == slang::ast::RangeSelectionKind::IndexedUp ||
                 kind == slang::ast::RangeSelectionKind::IndexedDown) {
        const BitIndex width = extract_constant_index(right);
        assert(width >= 0);
        left.visit(*this);
        const ExprId base = expr_stack_.back();
        expr_stack_.pop_back();
        const bool dir = (kind == slang::ast::RangeSelectionKind::IndexedUp);
        expr_stack_.push_back(builder_.create_part_select(data, base, width, dir, range.left, range.right));
      } else {
        throw std::logic_error("Unsupported range selection kind");
      }
    }

    void handle(const slang::ast::ConcatenationExpression &expr) {
      const size_t index = expr_stack_.size();
      this->visitDefault(expr);
      const size_t n = expr_stack_.size() - index;
      std::vector<ExprId> operands(n);
      for (size_t i = 0; i < n; i++) {
        operands[n - 1 - i] = expr_stack_.back();
        expr_stack_.pop_back();
      }
      expr_stack_.push_back(builder_.create_concat(std::move(operands), expr_sign(expr)));
    }

    void handle(const slang::ast::ReplicationExpression &expr) {
      const BitIndex rep = extract_constant_index(expr.count());
      if (rep < 0) {
        throw std::logic_error("Negative replication count");
      }
      expr.concat().visit(*this);
      ExprId body = expr_stack_.back();
      expr_stack_.pop_back();
      std::vector<ExprId> operands;
      operands.reserve(rep);
      for (size_t i = 0; i < static_cast<size_t>(rep); i++) {
        operands.push_back(body);
      }
      expr_stack_.push_back(builder_.create_concat(std::move(operands), expr_sign(expr)));
    }
    
    void handle(const slang::ast::ConditionalExpression &expr) {
      if (expr.conditions.size() != 1 || expr.conditions[0].pattern != nullptr) {
        throw std::logic_error("Unsupported conditional expression with pattern chain");
      }
      const size_t index = expr_stack_.size();
      this->visitDefault(expr);
      assert(expr_stack_.size() == index + 3);
      const ExprId else_id = expr_stack_.back();
      expr_stack_.pop_back();
      const ExprId then_id = expr_stack_.back();
      expr_stack_.pop_back();
      const ExprId cond = expr_stack_.back();
      expr_stack_.pop_back();
      expr_stack_.push_back(builder_.create_mux(cond, then_id, else_id));
    } 
    
    void handle(const slang::ast::BinaryExpression &expr) {
      this->visitDefault(expr);
      assert(expr_stack_.size() >= 2);
      ExprId rhs = expr_stack_.back();
      expr_stack_.pop_back();
      ExprId lhs = expr_stack_.back();
      expr_stack_.pop_back();
      ExprId id;
      switch (expr.op) {
      case slang::ast::BinaryOperator::LogicalOr:
        id = builder_.create_logical_or(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::LogicalAnd:
        id = builder_.create_logical_and(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::BinaryAnd:
        id = builder_.create_and({lhs, rhs});
        break;
      case slang::ast::BinaryOperator::BinaryOr:
        id = builder_.create_or({lhs, rhs});
        break;
      case slang::ast::BinaryOperator::BinaryXor:
        id = builder_.create_xor({lhs, rhs});
        break;
      case slang::ast::BinaryOperator::Add:
        id = builder_.create_add(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::Subtract:
        id = builder_.create_sub(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::Multiply:
        id = builder_.create_mul(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::Divide:
        id = builder_.create_div(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::Mod:
        id = builder_.create_mod(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::Power:
        id = builder_.create_pow(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::LogicalShiftLeft:
      case slang::ast::BinaryOperator::ArithmeticShiftLeft:
        id = builder_.create_shl(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::LogicalShiftRight:
        id = builder_.create_shr(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::ArithmeticShiftRight:
        id = builder_.create_ashr(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::Equality:
      case slang::ast::BinaryOperator::CaseEquality:
        id = builder_.create_eq(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::Inequality:
      case slang::ast::BinaryOperator::CaseInequality:
        id = builder_.create_neq(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::LessThan:
        id = builder_.create_lt(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::LessThanEqual:
        id = builder_.create_le(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::GreaterThan:
        id = builder_.create_gt(lhs, rhs);
        break;
      case slang::ast::BinaryOperator::GreaterThanEqual:
        id = builder_.create_ge(lhs, rhs);
        break;
      default:
        throw std::logic_error(
                               std::string("Unhandled binary operator: ")
                               + std::string(slang::ast::OpInfo::getText(expr.op)));
      }
      assert(id != kInvalidExprId);
      expr_stack_.push_back(id);
    }

    void handle(const slang::ast::UnaryExpression &expr) {
      this->visitDefault(expr);
      assert(!expr_stack_.empty());
      ExprId a = expr_stack_.back();
      expr_stack_.pop_back();
      ExprId id = kInvalidExprId;
      switch (expr.op) {
      case slang::ast::UnaryOperator::Plus:
        id = builder_.create_unary_plus(a);
        break;
      case slang::ast::UnaryOperator::Minus:
        id = builder_.create_unary_minus(a);
        break;
      case slang::ast::UnaryOperator::LogicalNot:
        id = builder_.create_logical_not(a);
        break;
      case slang::ast::UnaryOperator::BitwiseNot:
        id = builder_.create_bitwise_not(a);
        break;
      case slang::ast::UnaryOperator::BitwiseAnd:
        id = builder_.create_and_reduce(a);
        break;
      case slang::ast::UnaryOperator::BitwiseOr:
        id = builder_.create_or_reduce(a);
        break;
      case slang::ast::UnaryOperator::BitwiseXor:
        id = builder_.create_xor_reduce(a);
        break;
      case slang::ast::UnaryOperator::BitwiseNand:
        id = builder_.create_and_reduce(a);
        id = builder_.create_logical_not(id);
        break;
      case slang::ast::UnaryOperator::BitwiseNor:
        id = builder_.create_or_reduce(a);
        id = builder_.create_logical_not(id);
        break;
      case slang::ast::UnaryOperator::BitwiseXnor:
        id = builder_.create_xor_reduce(a);
        id = builder_.create_logical_not(id);
        break;
      case slang::ast::UnaryOperator::Preincrement:
        id = builder_.create_preinc(a);
        break;
      case slang::ast::UnaryOperator::Predecrement:
      case slang::ast::UnaryOperator::Postincrement:
      case slang::ast::UnaryOperator::Postdecrement:
        throw std::logic_error("inc/dec unary operators are not yet supported");
      }
      assert(id != kInvalidExprId);
      expr_stack_.push_back(id);
    }

    void handle(const slang::ast::SimpleAssignmentPatternExpression &expr) {
      const size_t index = expr_stack_.size();
      this->visitDefault(expr);
      const size_t n = expr_stack_.size() - index;
      std::vector<ExprId> operands(n);
      for (size_t i = 0; i < n; i++) {
        operands[n - 1 - i] = expr_stack_.back(); // restore original element order
        expr_stack_.pop_back();
      }
      expr_stack_.push_back(builder_.create_gather(std::move(operands)));
    }

    // TODO: handle compound assignment here

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

  void get_width_sign_range(const slang::ast::Type &type, SignalWidth &width, bool &sign, slang::ConstantRange &range) {
    if (type.isUnpackedArray()) {
      const auto &ct = type.getCanonicalType();
      if (ct.kind != slang::ast::SymbolKind::FixedSizeUnpackedArrayType) {
        throw std::logic_error("Unsupported dynamic size unpacked array");
      }
      const auto &arr = ct.as<slang::ast::FixedSizeUnpackedArrayType>();
      range = arr.range;
      width = (range.left >= range.right) ? (range.left - range.right + 1) : (range.right - range.left + 1);
      sign = false;
    } else {
      range = type.getFixedRange();
      width = type.getBitstreamWidth();
      sign = type.isSigned();
    }
  }
    
  template <typename EmitFn>
  void lower_lhs_assignment(const slang::ast::Expression &whole_lhs, ExprId rhs_id, SignalWidth rhs_width, ExprBuilder &expr_builder, EmitFn &&record) {
    assert(rhs_width > 0);
    BitIndex remaining = rhs_width;
    auto get_rhs_id = [&](const slang::ast::Expression &lhs) -> ExprId {
      ExprId expr_id = rhs_id;
      SignalWidth width;
      bool sign;
      get_width_sign(*lhs.type, width, sign);
      assert(remaining >= static_cast<BitIndex>(width));
      if (width != rhs_width) {
        const BitIndex left = remaining - 1;
        const BitIndex right = remaining - width;
        expr_id = expr_builder.create_range(rhs_id, left, right, static_cast<BitIndex>(rhs_width - 1), 0);
      }
      remaining -= width;
      return expr_id;
    };
    std::vector<const slang::ast::Expression*> lhs_stack{&whole_lhs};
    while (!lhs_stack.empty()) {
      const slang::ast::Expression &lhs = *lhs_stack.back();
      lhs_stack.pop_back();
      std::string output_name;
      ExprId expr_id = kInvalidExprId;
      if (lhs.kind == slang::ast::ExpressionKind::NamedValue) {
        expr_id = get_rhs_id(lhs);
        output_name = extract_named_value(lhs);
      } else if (lhs.kind == slang::ast::ExpressionKind::ElementSelect) {
        expr_id = get_rhs_id(lhs);
        const auto &sel = lhs.as<slang::ast::ElementSelectExpression>();
        output_name = extract_named_value(sel.value());
        const ExprId index_id = build_expr(sel.selector(), expr_builder);
        const auto &type = *sel.value().type;
        SignalWidth width;
        bool sign;
        slang::ConstantRange range;
        get_width_sign_range(type, width, sign, range);
        expr_id = expr_builder.assign_select(expr_id, index_id, output_name, width, sign, range.left, range.right);
      } else if (lhs.kind == slang::ast::ExpressionKind::RangeSelect) {
        expr_id = get_rhs_id(lhs);
        const auto &sel = lhs.as<slang::ast::RangeSelectExpression>();
        output_name = extract_named_value(sel.value());
        const auto kind = sel.getSelectionKind();
        const auto &left = sel.left();
        const auto &right = sel.right();
        const auto &type = *sel.value().type;
        SignalWidth width;
        bool sign;
        slang::ConstantRange range;
        get_width_sign_range(type, width, sign, range);
        if (kind == slang::ast::RangeSelectionKind::Simple) {
          const BitIndex left_sw = extract_constant_index(left);
          const BitIndex right_sw = extract_constant_index(right);
          expr_id = expr_builder.assign_range(expr_id, left_sw, right_sw, output_name, width, sign, range.left, range.right);
        } else if (kind == slang::ast::RangeSelectionKind::IndexedUp ||
                   kind == slang::ast::RangeSelectionKind::IndexedDown) {
          const BitIndex slice_width = extract_constant_index(right);
          assert(slice_width >= 0);
          const ExprId base_id = build_expr(left, expr_builder);
          const bool dir = (kind == slang::ast::RangeSelectionKind::IndexedUp);
          expr_id = expr_builder.assign_part_select(expr_id, base_id, slice_width, dir, output_name, width, sign, range.left, range.right);
        } else {
          throw std::logic_error("Unsupported range selection kind");
        }
      } else if (lhs.kind == slang::ast::ExpressionKind::Concatenation) {
        const auto &cat = lhs.as<slang::ast::ConcatenationExpression>();
        for (auto it = cat.operands().rbegin(); it != cat.operands().rend(); ++it) {
          lhs_stack.push_back(*it);
        }
        continue;
      } else {
        throw std::logic_error("Unsupported LHS");
      }
      record(output_name, expr_id);
    }
    assert(remaining == 0);
  }

  template <typename Builder>
  class SlangStmtLoweringVisitor final
    : public slang::ast::ASTVisitor<SlangStmtLoweringVisitor<Builder>, true, false, false, true> {
  private:
    Builder &builder_;

  public:
    explicit SlangStmtLoweringVisitor(Builder &builder) : builder_(builder) {}

    template<typename T>
    void handle(const T&) {
      throw std::logic_error(
                             std::string("Unhandled AST node: ") + typeid(T).name()
                             );
    }

    void handle(const slang::ast::EmptyStatement &) {
      return;
    }

    void handle(const slang::ast::VariableDeclStatement &stmt) {
      const auto &symbol = stmt.symbol;
      const std::string name(symbol.name);
      builder_.add_local_variable(name);
      if (const auto *init = symbol.getInitializer()) {
        ExprId expr_id = build_expr(*init, builder_.get_expr_builder());
        builder_.get_expr_builder().update_value(name, expr_id);
        builder_.output_names().push_back(name);
        builder_.output_nonblocking().push_back(false);
        builder_.output_ids().push_back(expr_id);
      }
    }
    
    void handle(const slang::ast::ExpressionStatement &stmt) {
      if (stmt.expr.kind == slang::ast::ExpressionKind::Call) {
        const auto &call = stmt.expr.as<slang::ast::CallExpression>();
        if (call.isSystemCall()) {
          // TODO: create warning standard in this repo
          std::cerr << "warning: ignoring system task call in synthesis lowering: " << call.getSubroutineName() << "\n";
          return;
        }
      }
      if (stmt.expr.kind != slang::ast::ExpressionKind::Assignment) {
	throw std::logic_error("Non-assignment expression statement is unsupported");
      }
      const auto &assign = stmt.expr.as<slang::ast::AssignmentExpression>();
      assert(!assign.isCompound()); // TODO: we need to handle this later
      ExprId rhs_id = build_expr(assign.right(), builder_.get_expr_builder());
      const SignalWidth rhs_width = builder_.get_expr_builder().get_width(rhs_id);
      const bool nonblocking = assign.isNonBlocking();
      std::unordered_map<std::string, ExprId> to_restore;
      lower_lhs_assignment(assign.left(), rhs_id, rhs_width, builder_.get_expr_builder(), [&](const std::string &output_name, ExprId expr_id) {
        if (nonblocking && !to_restore.count(output_name)) {
          to_restore[output_name] = builder_.get_expr_builder().get_current_value(output_name);
        }
        builder_.get_expr_builder().update_value(output_name, expr_id);
        builder_.output_names().push_back(output_name);
        builder_.output_nonblocking().push_back(nonblocking);
        builder_.output_ids().push_back(expr_id);
      });
      for (const auto &kv : to_restore) {
        builder_.get_expr_builder().update_value(kv.first, kv.second);
      }
    }
    
    void handle(const slang::ast::ConditionalStatement &stmt) {
      std::vector<ExprId> cond_ids;
      for (const auto &cond : stmt.conditions) {
	ExprId cond_id = build_expr(*cond.expr, builder_.get_expr_builder());
	cond_ids.push_back(cond_id);
      }
      assert(!cond_ids.empty());
      ExprId cond_id = kInvalidExprId;
      if (cond_ids.size() > 1) {
	cond_id = builder_.get_expr_builder().create_and(std::move(cond_ids));
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
      ExprId case_id = build_expr(stmt.expr, builder_.get_expr_builder());
      std::vector<ExprId> case_values;
      for (auto &item : stmt.items) {
	std::vector<ExprId> values;
	for (auto *v : item.expressions) {
	  values.push_back(build_expr(*v, builder_.get_expr_builder()));
	}
	ExprId value_id = kInvalidExprId;
	if (values.size() > 1) {
	  value_id = builder_.get_expr_builder().create_list(std::move(values));
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

    void handle(const slang::ast::ForLoopStatement& stmt) {
      if (!stmt.loopVars.empty()) {
        builder_.create_context();
        for (auto var : stmt.loopVars) {
          const std::string name(var->name);
          builder_.add_local_variable(name);
          if (const auto *init = var->getInitializer()) {
            ExprId expr_id = build_expr(*init, builder_.get_expr_builder());
            builder_.get_expr_builder().update_value(name, expr_id);
            builder_.output_names().push_back(name);
            builder_.output_nonblocking().push_back(false);
            builder_.output_ids().push_back(expr_id);
          }
        }
      }
      for (auto init : stmt.initializers) {
        if (!init) {
          continue;
        }
        if (init->kind != slang::ast::ExpressionKind::Assignment) {
          throw std::logic_error("unsupported for-loop initializer");
        }
        const auto &assign = init->as<slang::ast::AssignmentExpression>();
        if (assign.isCompound()) {
          throw std::logic_error("compound for-loop initializer is unsupported");
        }
        assert(!assign.isNonBlocking());
        ExprId rhs_id = build_expr(assign.right(), builder_.get_expr_builder());
        const SignalWidth rhs_width = builder_.get_expr_builder().get_width(rhs_id);
        lower_lhs_assignment(assign.left(), rhs_id, rhs_width, builder_.get_expr_builder(), [&](const std::string &output_name, ExprId expr_id) {
          builder_.get_expr_builder().update_value(output_name, expr_id);
          builder_.output_names().push_back(output_name);
          builder_.output_nonblocking().push_back(false);
          builder_.output_ids().push_back(expr_id);
        });
      }
      for (size_t iter = 0; ; iter++) {
        // TODO: warn if this iterates too many times
        if (!stmt.stopExpr) {
          // TODO: handle break/continue
          throw std::logic_error("for-loop without stop condition is unsupported");
        }
        ExprId stop_id = build_expr(*stmt.stopExpr, builder_.get_expr_builder());
        if (!builder_.get_expr_builder().evaluate(stop_id)) {
          break;
        }
        stmt.body.visit(*this);
        for (auto step : stmt.steps) {
          if (!step) {
            continue;
          }
          if (step->kind != slang::ast::ExpressionKind::Assignment) {
            throw std::logic_error("unsupported for-loop step");
          }
          const auto &assign = step->as<slang::ast::AssignmentExpression>();
          if (assign.isCompound()) {
            throw std::logic_error("compound is unsupported");
          }
          assert(!assign.isNonBlocking());
          ExprId rhs_id = build_expr(assign.right(), builder_.get_expr_builder());
          const SignalWidth rhs_width = builder_.get_expr_builder().get_width(rhs_id);
          lower_lhs_assignment(assign.left(), rhs_id, rhs_width, builder_.get_expr_builder(), [&](const std::string &output_name, ExprId expr_id) {
            builder_.get_expr_builder().update_value(output_name, expr_id);
            builder_.output_names().push_back(output_name);
            builder_.output_nonblocking().push_back(false);
            builder_.output_ids().push_back(expr_id);
          });
        }
      }
      if (!stmt.loopVars.empty()) {
        builder_.merge_context();
      }
    }
    
    void handle(const slang::ast::StatementList &stmt) {
      this->visitDefault(stmt);
    }
    
    void handle(const slang::ast::BlockStatement &stmt) {
      builder_.create_context();
      this->visitDefault(stmt);
      builder_.merge_context();
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
      auto add_event = [&](const slang::ast::SignalEventControl &ev) {
        const ExprId expr_id = build_expr(ev.expr, builder_.get_expr_builder());
        const ExprId iff_id = ev.iffCondition ? build_expr(*ev.iffCondition, builder_.get_expr_builder()) : kInvalidExprId;
        const bool pos = ev.edge == slang::ast::EdgeKind::PosEdge || ev.edge == slang::ast::EdgeKind::BothEdges;
        const bool neg = ev.edge == slang::ast::EdgeKind::NegEdge || ev.edge == slang::ast::EdgeKind::BothEdges;
        builder_.add_timing(expr_id, iff_id, pos, neg);
      };
      const auto &timing = stmt.timing;
      switch (timing.kind) {
      case slang::ast::TimingControlKind::SignalEvent:
        add_event(timing.as<slang::ast::SignalEventControl>());
        break;
      case slang::ast::TimingControlKind::EventList: {
        const auto &list = timing.as<slang::ast::EventListControl>();
        for (const auto *tc : list.events) {
          if (tc->kind != slang::ast::TimingControlKind::SignalEvent) {
            throw std::logic_error("Unsupported timing event in list");
          }
          add_event(tc->as<slang::ast::SignalEventControl>());
        }
        break;
      }
      case slang::ast::TimingControlKind::ImplicitEvent:
        if (!builder_.is_undecided() || builder_.has_timing()) {
          throw std::logic_error("Invalid implicit event timing");
        }
        builder_.set_comb_or_latch();
        break;
      default:
        throw std::logic_error("Unsupported timing control kind");
      }
      this->visitDefault(stmt);
      if (builder_.is_undecided()) {
        throw std::logic_error("TimedStatement did not set policy");
      }
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
      ExprBuilder expr_builder(builder_.get_expr_graph(module_id, node_id));
      const ExprId expr_id = build_expr(expr, expr_builder);
      expr_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
        builder_.add_node_input_spec(module_id, node_id, name, width, sign);
      });
      builder_.add_node_output_expr(module_id, node_id, std::move(output_name), expr_id, true);
      return node_id;
    }

    void create_variable(const slang::ast::ValueSymbol &symbol, bool net) {
      const auto &type = symbol.getType().getCanonicalType();
      if (type.isUnpackedArray()) {
        std::vector<SignalWidth> dims;
        const slang::ast::Type *t = &type;
        while (t->kind == slang::ast::SymbolKind::FixedSizeUnpackedArrayType) {
          const auto &arr = t->as<slang::ast::FixedSizeUnpackedArrayType>();
          const auto range = arr.range;
          const BitIndex width = (range.left >= range.right) ? (range.left - range.right + 1) : (range.right - range.left + 1);
          assert(width >= 0);
          dims.push_back(width);
          t = &arr.elementType.getCanonicalType();
        }
        const auto &elem = *t;
        const SignalWidth width = elem.getBitstreamWidth();
        const bool sign = elem.isSigned();
        bool reg = false;
        if (!net) {
          if (slang::ast::IntegralType::isKind(elem.kind)) {
            reg = elem.as<slang::ast::IntegralType>().isDeclaredReg();
          }
        }
        builder_.create_packed_variable(current_module_id(), std::string(symbol.name), std::move(dims), width, sign, net, reg);
      } else {
        const SignalWidth width = type.getBitstreamWidth();
        const bool sign = type.isSigned();
        bool reg = false;
        if (!net) {
          if (slang::ast::IntegralType::isKind(type.kind)) {
            reg = type.as<slang::ast::IntegralType>().isDeclaredReg();
          }
        }
        builder_.create_variable(current_module_id(), std::string(symbol.name), width, sign, net, reg);
      }
      // TODO: implement debugging (mismatch, unused, or nondeclared)
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

    void handle(const slang::ast::ParameterSymbol &symbol) {
      const auto* init = symbol.getInitializer();
      if (!init) {
        throw std::logic_error("Parameter without initializer: " + std::string(symbol.name));
      }
      const ModuleId module_id = current_module_id();
      const NodeId node_id = builder_.create_operation(module_id);
      ExprBuilder expr_builder(builder_.get_expr_graph(module_id, node_id));
      const ExprId expr_id = build_expr(*init, expr_builder);
      builder_.add_node_output_expr(module_id, node_id, std::string(symbol.name), expr_id, true);
      create_variable(symbol, false);
      // TODO: probably we want parameters directly assigned as a constant, by passing a list to expr builder
    }
    
    void handle(const slang::ast::PortSymbol &symbol) {
      this->visitDefault(symbol);
      if (symbol.direction == slang::ast::ArgumentDirection::InOut) {
        throw std::logic_error("InOut ports are not supported");
      }
      if (symbol.direction == slang::ast::ArgumentDirection::Ref) {
        throw std::logic_error("Ref ports are not supported");
      }
      // TODO: handle unpacked
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
      // TODO: sanitize multiple assignment on packed/unpacked variables
      builder_.insert_ffs(module_id);
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
	  if (port.direction == slang::ast::ArgumentDirection::In) {
            assert(expr);
	    if (expr->kind != slang::ast::ExpressionKind::NamedValue) {
	      const NodeId input_id = create_expr_node(*expr);
	      builder_.add_node_input(module_id, node_id, input_id);
	    } else {
              // TODO: handle unpacked
	      builder_.add_node_input_spec(module_id, node_id, extract_named_value(*expr),
                                           expr_width(*expr), expr_sign(*expr));
	    }
	  } else if (port.direction == slang::ast::ArgumentDirection::Out) {
            if (!expr) {
              builder_.add_node_output(module_id, node_id, "", 0, false);
              continue;
            }
            assert(expr->kind == slang::ast::ExpressionKind::Assignment);
            const auto &assign = expr->as<slang::ast::AssignmentExpression>();
            assert(assign.right().kind == slang::ast::ExpressionKind::EmptyArgument);
            const auto &lhs = assign.left();
            const SignalWidth rhs_width = port_width(port); // TODO: handle unpacked array
            const bool rhs_sign = port_sign(port);
            if (lhs.kind == slang::ast::ExpressionKind::NamedValue) {
              builder_.add_node_output(module_id, node_id, extract_named_value(lhs), rhs_width, rhs_sign);
            } else {
              const std::string temporary_name = builder_.generate_temporary_name();
              const PortIndex port_idx = builder_.add_node_output(module_id, node_id, temporary_name, rhs_width, rhs_sign);
              const NodeId op_id = builder_.create_operation(module_id);
              ExprBuilder expr_builder(builder_.get_expr_graph(module_id, op_id));
              ExprId rhs_id = expr_builder.find_or_create_input(temporary_name, rhs_width, rhs_sign);
              std::unordered_map<std::string, ExprId> to_store;
              lower_lhs_assignment(lhs, rhs_id, rhs_width, expr_builder, [&](const std::string &output_name, ExprId expr_id) {
                expr_builder.update_value(output_name, expr_id);
                to_store[output_name] = expr_id;
              });
              builder_.add_node_input(module_id, op_id, node_id, port_idx);
              for (const auto &kv : to_store) {
                builder_.add_node_output_expr(module_id, op_id, kv.first, kv.second, true);
              }
            }
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
      const ModuleId module_id = current_module_id();
      const NodeId node_id = builder_.create_operation(module_id);
      ExprBuilder expr_builder(builder_.get_expr_graph(module_id, node_id));
      ExprId rhs_id = build_expr(assign_expr.right(), expr_builder);
      const SignalWidth rhs_width = expr_builder.get_width(rhs_id);
      std::unordered_map<std::string, ExprId> to_store;
      lower_lhs_assignment(assign_expr.left(), rhs_id, rhs_width, expr_builder, [&](const std::string &output_name, ExprId expr_id) {
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
    
    void handle(const slang::ast::RootSymbol &symbol) {
      this->visitDefault(symbol);
    }

    void handle(const slang::ast::CompilationUnitSymbol &symbol) {
      this->visitDefault(symbol);
    }

    void handle(const slang::ast::VariableSymbol &symbol) {
      create_variable(symbol, false);
    }
    
    void handle(const slang::ast::NetSymbol &symbol) {
      create_variable(symbol, true);
      const auto *init = symbol.getInitializer();
      if (!init) {
        return;
      }
      const ModuleId module_id = current_module_id();
      const NodeId node_id = builder_.create_operation(module_id);
      ExprBuilder expr_builder(builder_.get_expr_graph(module_id, node_id));
      ExprId rhs_id = build_expr(*init, expr_builder);
      expr_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
        builder_.add_node_input_spec(module_id, node_id, name, width, sign);
      });
      builder_.add_node_output_expr(module_id, node_id, std::string(symbol.name), rhs_id, true);
    }
    
    void handle(const slang::ast::ProceduralBlockSymbol &symbol) {
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
      SlangStmtLoweringVisitor<StmtBuilder> stmt_visitor(stmt_builder);
      const slang::ast::Statement& stmt = symbol.getBody();
      stmt.visit(stmt_visitor);
      stmt_builder.for_each_input([&](const std::string &name, SignalWidth width, bool sign) {
        builder_.add_node_input_spec(module_id, node_id, name, width, sign);
      });
      // TODO: skip integers to be assigned (what else should we skip?)
      if(!stmt_builder.is_ff()) {
        // TODO: latch inference is deferred
        stmt_builder.for_each_output([&](const std::string &name, ExprId expr_id) {
          builder_.add_node_output_expr(module_id, node_id, name, expr_id, stmt_builder.is_comb());
        });
        return;
      }
      std::vector<std::pair<std::string, ExprId>> outputs;
      stmt_builder.for_each_output([&](const std::string &name, ExprId expr_id) {
        outputs.emplace_back(name, expr_id);
      });
      std::string clk_name, rst_name;
      SignalWidth clk_width, rst_width;
      bool clk_sign, rst_sign;
      EdgeKind clk_edge, rst_edge;
      stmt_builder.get_timing_spec(outputs, clk_name, clk_width, clk_sign, clk_edge, rst_name, rst_width, rst_sign, rst_edge);
      for (const auto &kv : outputs) {
        const PortIndex port_idx = builder_.add_node_output_expr(module_id, node_id, kv.first, kv.second, stmt_builder.is_comb());
        builder_.record_ff(module_id, kv.first, {clk_name, clk_width, clk_sign}, clk_edge, {rst_name, rst_width, rst_sign}, rst_edge, node_id, port_idx);
      }
    }

    void handle(const slang::ast::SubroutineSymbol &symbol) {
      if (symbol.subroutineKind != slang::ast::SubroutineKind::Function) {
        std::cerr << "warning: ignoring task in synthesis lowering: " << symbol.name << "\n";
        return;
      }
      // TODO: it is better to remove dependency on tig structure; use builder api to create a subroutine
      Tig::Subroutine subr;
      subr.subr_ptr = &symbol;
      subr.name = std::string(symbol.name);
      for (auto *arg : symbol.getArguments()) {
        // TODO: handle packed/unpacked array
        if (arg->direction != slang::ast::ArgumentDirection::In) {
          throw std::logic_error("Only input formals are supported in function lowering: " + std::string(symbol.name) + "." + std::string(arg->name));
        }
        const auto &type = arg->getType();
        subr.inputs.emplace_back(Tig::Subroutine::Port{std::string(arg->name), type.getBitstreamWidth(), type.isSigned()});
      }
      StmtBuilder stmt_builder(subr.expr_graph);
      SlangStmtLoweringVisitor<StmtBuilder> stmt_visitor(stmt_builder);
      symbol.getBody().visit(stmt_visitor);
      const ExprId ret = stmt_builder.get_expr_builder().get_current_value(symbol.name);
      if (ret == kInvalidExprId) {
        throw std::logic_error("Function has no return assignment: " + std::string(symbol.name));
      }
      subr.expr_root = ret;
      builder_.add_subroutine(std::move(subr)); // add API
    }
  };
  
  template <typename Builder>
  void lower_slang_ast_to_ir(const slang::ast::RootSymbol &root, Builder &builder) {
    SlangLoweringVisitor<Builder> visitor(builder);
    root.visit(visitor);
    builder.flatten_calls();
  }
  
} // namespace abys::ir
