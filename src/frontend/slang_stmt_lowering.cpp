#include "slang_lowering_internal.h"

namespace abys::frontend {

class SlangStmtLoweringVisitor final
    : public slang::ast::ASTVisitor<SlangStmtLoweringVisitor, true, false, false, true> {
private:
  StmtBuilder &builder_;
  SlangLoweringContext &context_;
  const PragmaMap &pragmas_;

public:
  explicit SlangStmtLoweringVisitor(StmtBuilder &builder, SlangLoweringContext &context,
                                    const PragmaMap &pragmas)
      : builder_(builder), context_(context), pragmas_(pragmas) {}

  template <typename T> void handle(const T &) {
    throw std::logic_error(std::string("Unhandled AST node: ") + typeid(T).name());
  }

  void handle(const slang::ast::EmptyStatement &) {}

  void handle(const slang::ast::VariableDeclStatement &stmt) {
    const auto &symbol = stmt.symbol;
    const std::string name(symbol.name);
    builder_.add_local_variable(name);
    if (const auto *init = symbol.getInitializer()) {
      ExprId expr_id = build_expr(*init, builder_.get_expr_builder(), context_);
      builder_.get_expr_builder().update_value(name, expr_id);
      builder_.scheduled_assignments()[name] = expr_id;
      builder_.output_names().push_back(name);
      builder_.output_nonblocking().push_back(false);
      builder_.output_ids().push_back(expr_id);
    }
  }

  void handle(const slang::ast::ExpressionStatement &stmt) {
    if (stmt.expr.kind == slang::ast::ExpressionKind::Call) {
      const auto &call = stmt.expr.as<slang::ast::CallExpression>();
      if (call.isSystemCall()) {
        using slang::parsing::KnownSystemName;
        switch (call.getKnownSystemName()) {
        case KnownSystemName::Display:
        case KnownSystemName::DisplayH:
        case KnownSystemName::Write:
        case KnownSystemName::WriteB:
        case KnownSystemName::WriteO:
        case KnownSystemName::WriteH:
        case KnownSystemName::Strobe:
        case KnownSystemName::StrobeB:
        case KnownSystemName::StrobeO:
        case KnownSystemName::StrobeH:
        case KnownSystemName::Monitor:
        case KnownSystemName::MonitorB:
        case KnownSystemName::MonitorO:
        case KnownSystemName::MonitorH:
        case KnownSystemName::FDisplay:
        case KnownSystemName::FDisplayB:
        case KnownSystemName::FDisplayO:
        case KnownSystemName::FDisplayH:
        case KnownSystemName::FWrite:
        case KnownSystemName::FWriteB:
        case KnownSystemName::FWriteO:
        case KnownSystemName::FWriteH:
        case KnownSystemName::FStrobe:
        case KnownSystemName::FStrobeB:
        case KnownSystemName::FStrobeO:
        case KnownSystemName::FStrobeH:
        case KnownSystemName::FMonitor:
        case KnownSystemName::FMonitorB:
        case KnownSystemName::FMonitorO:
        case KnownSystemName::FMonitorH:
        case KnownSystemName::Finish:
        case KnownSystemName::Stop:
        case KnownSystemName::Info:
        case KnownSystemName::Warning:
        case KnownSystemName::Error:
        case KnownSystemName::Fatal:
        case KnownSystemName::FOpen:
        case KnownSystemName::FClose:
        case KnownSystemName::FFlush:
          std::cerr << "warning: ignoring non-synthesizable system call: "
                    << call.getSubroutineName() << '\n';
          return;
        case KnownSystemName::ReadMemB:
        case KnownSystemName::ReadMemH:
        case KnownSystemName::WriteMemB:
        case KnownSystemName::WriteMemH:
        default:
          throw std::logic_error("Unsupported system call: " +
                                 std::string(call.getSubroutineName()));
        }
      }
    }
    if (stmt.expr.kind != slang::ast::ExpressionKind::Assignment) {
      throw std::logic_error("Non-assignment expression statement is unsupported");
    }
    const auto &assign = stmt.expr.as<slang::ast::AssignmentExpression>();
    assert(!assign.isCompound()); // TODO: we need to handle this later
    ExprId rhs_id = build_expr(assign.right(), builder_.get_expr_builder(), context_);
    const SignalWidth rhs_width = builder_.get_expr_builder().get_width(rhs_id);
    const bool nonblocking = assign.isNonBlocking();
    std::unordered_map<std::string, ExprId> to_restore;
    lower_lhs_assignment(
        assign.left(), rhs_id, rhs_width, builder_.get_expr_builder(), context_,
        &builder_.scheduled_assignments(), [&](const std::string &output_name, ExprId expr_id) {
          if (nonblocking && !to_restore.contains(output_name)) {
            to_restore[output_name] = builder_.get_expr_builder().get_current_value(output_name);
          }
          builder_.get_expr_builder().update_value(output_name, expr_id);
          builder_.scheduled_assignments()[output_name] = expr_id;
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
      ExprId cond_id = build_expr(*cond.expr, builder_.get_expr_builder(), context_);
      cond_ids.push_back(cond_id);
    }
    assert(!cond_ids.empty());
    ExprId cond_id = kInvalidExprId;
    if (cond_ids.size() > 1) {
      cond_id = builder_.get_expr_builder().create_and(std::move(cond_ids));
    } else {
      cond_id = cond_ids[0];
    }
    if (auto cond_value = builder_.get_expr_builder().try_evaluate(cond_id)) {
      if (*cond_value) {
        stmt.ifTrue.visit(*this);
      } else if (stmt.ifFalse) {
        stmt.ifFalse->visit(*this);
      }
      return;
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
    ExprId case_id = build_expr(stmt.expr, builder_.get_expr_builder(), context_);
    std::vector<ExprId> case_values;
    for (const auto &item : stmt.items) {
      std::vector<ExprId> values;
      for (const auto *value_expression : item.expressions) {
        values.push_back(build_expr(*value_expression, builder_.get_expr_builder(), context_));
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
    bool full_case = false;
    if (stmt.syntax) {
      auto it = pragmas_.by_node.find(static_cast<const slang::syntax::SyntaxNode *>(stmt.syntax));
      full_case = it != pragmas_.by_node.end() && it->second.full_case;
    }
    if (full_case && stmt.defaultCase) {
      std::cerr << "warning: ignoring full_case on case statement with default\n";
    }
    // TODO: propagate parallel_case
    builder_.merge_case(case_id, case_values, index, full_case && !stmt.defaultCase);
  }

  void handle(const slang::ast::ForLoopStatement &stmt) {
    if (!stmt.loopVars.empty()) {
      builder_.create_context();
      for (const auto *var : stmt.loopVars) {
        const std::string name(var->name);
        builder_.add_local_variable(name);
        if (const auto *init = var->getInitializer()) {
          ExprId expr_id = build_expr(*init, builder_.get_expr_builder(), context_);
          builder_.get_expr_builder().update_value(name, expr_id);
          builder_.scheduled_assignments()[name] = expr_id;
          builder_.output_names().push_back(name);
          builder_.output_nonblocking().push_back(false);
          builder_.output_ids().push_back(expr_id);
        }
      }
    }
    for (const auto *init : stmt.initializers) {
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
      ExprId rhs_id = build_expr(assign.right(), builder_.get_expr_builder(), context_);
      const SignalWidth rhs_width = builder_.get_expr_builder().get_width(rhs_id);
      lower_lhs_assignment(assign.left(), rhs_id, rhs_width, builder_.get_expr_builder(), context_,
                           &builder_.scheduled_assignments(),
                           [&](const std::string &output_name, ExprId expr_id) {
                             builder_.get_expr_builder().update_value(output_name, expr_id);
                             builder_.scheduled_assignments()[output_name] = expr_id;
                             builder_.output_names().push_back(output_name);
                             builder_.output_nonblocking().push_back(false);
                             builder_.output_ids().push_back(expr_id);
                           });
    }
    for (size_t iter = 0;; ++iter) {
      // TODO: warn if this iterates too many times
      if (!stmt.stopExpr) {
        // TODO: handle break/continue
        throw std::logic_error("for-loop without stop condition is unsupported");
      }
      ExprId stop_id = build_expr(*stmt.stopExpr, builder_.get_expr_builder(), context_);
      if (!builder_.get_expr_builder().evaluate(stop_id)) {
        break;
      }
      stmt.body.visit(*this);
      for (const auto *step : stmt.steps) {
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
        ExprId rhs_id = build_expr(assign.right(), builder_.get_expr_builder(), context_);
        const SignalWidth rhs_width = builder_.get_expr_builder().get_width(rhs_id);
        const bool rhs_sign = builder_.get_expr_builder().get_sign(rhs_id);
        const int rhs_value =
            builder_.get_expr_builder().evaluate(rhs_id); // TODO: sanitize type of this evaluate
        const slang::SVInt rhs_sv(rhs_width, static_cast<uint64_t>(rhs_value), rhs_sign);
        rhs_id = builder_.get_expr_builder().find_or_create_const(
            rhs_sv.toString(slang::LiteralBase::Binary), rhs_width, rhs_sign);
        lower_lhs_assignment(assign.left(), rhs_id, rhs_width, builder_.get_expr_builder(),
                             context_, &builder_.scheduled_assignments(),
                             [&](const std::string &output_name, ExprId expr_id) {
                               builder_.get_expr_builder().update_value(output_name, expr_id);
                               builder_.scheduled_assignments()[output_name] = expr_id;
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

  void handle(const slang::ast::StatementList &stmt) { this->visitDefault(stmt); }

  void handle(const slang::ast::BlockStatement &stmt) {
    builder_.create_context();
    this->visitDefault(stmt);
    builder_.merge_context();
  }

  void handle(const slang::ast::TimedStatement &stmt) {
    if (!builder_.is_root_context()) {
      throw std::logic_error("TimedStatement in a non-root context");
    }
    if (!builder_.is_ff() && !builder_.is_undecided()) {
      throw std::logic_error("TimedStatement in always_comb or always_latch");
    }
    if (builder_.has_timing()) {
      throw std::logic_error("Nested TimedStatement");
    }
    auto add_event = [&](const slang::ast::SignalEventControl &ev) {
      const ExprId expr_id = build_expr(ev.expr, builder_.get_expr_builder(), context_);
      const ExprId iff_id =
          ev.iffCondition ? build_expr(*ev.iffCondition, builder_.get_expr_builder(), context_)
                          : kInvalidExprId;
      const bool pos =
          ev.edge == slang::ast::EdgeKind::PosEdge || ev.edge == slang::ast::EdgeKind::BothEdges;
      const bool neg =
          ev.edge == slang::ast::EdgeKind::NegEdge || ev.edge == slang::ast::EdgeKind::BothEdges;
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

void lower_statement(const slang::ast::Statement &statement, StmtBuilder &builder,
                     SlangLoweringContext &context, const PragmaMap &pragmas) {
  SlangStmtLoweringVisitor visitor(builder, context, pragmas);
  statement.visit(visitor);
}

} // namespace abys::frontend
