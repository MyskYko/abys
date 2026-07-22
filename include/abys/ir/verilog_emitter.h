#pragma once

#include <map>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "abys/ir/tig.h"

namespace abys::ir {

class VerilogEmitter {
public:
  explicit VerilogEmitter(const Tig &design);
  void emit(std::ostream &os) const;
  // std::string emit_to_string(const Design& design) const;

  using Module = Tig::Module;

private:
  // use shift/mask selects for non-direct expression bases for yosys compatibility
  static constexpr bool kUseShiftMaskForExpressionSelects = true;
  static constexpr bool kUsePartialAssignmentForMaskedAssign = true;

  const Tig &design_;

  void emit_module(const Module &module, std::ostream &os) const;
  void emit_module_header(const Module &module, std::ostream &os) const;
  void emit_signal_decls(const Module &module, std::ostream &os) const;
  void emit_instances(const Module &module, std::ostream &os) const;
  void emit_combinational(const Module &module, std::ostream &os) const;
  void emit_sequential(const Module &module, std::ostream &os) const;
  void emit_output_binds(const Module &module, std::ostream &os) const;
  void emit_module_footer(std::ostream &os) const;

  void emit_expr(std::string_view lhs, bool is_nonblocking, bool is_merge,
                 const ExprGraph &expr_graph, ExprId id, std::ostream &os, std::string_view indent,
                 const std::unordered_map<std::string, bool> *assumptions = nullptr) const;
  void emit_exprs(const std::vector<std::string> &lhs_names, bool is_nonblocking, bool is_merge,
                  const ExprGraph &expr_graph, const std::vector<ExprId> &expr_ids,
                  std::ostream &os, std::string_view indent,
                  const std::unordered_map<std::string, bool> *assumptions = nullptr) const;
  void emit_expr_unpacked(std::string lhs, bool is_nonblocking, bool is_merge,
                          const ExprGraph &expr_graph, ExprId id,
                          std::map<ExprId, std::string> &names, std::ostream &decl_os,
                          std::ostream &os, std::ostream &assign_os, std::string_view indent,
                          const std::unordered_map<std::string, bool> *assumptions = nullptr) const;
  std::string
  emit_expr_packed(const ExprGraph &expr_graph, ExprId id, std::map<ExprId, std::string> &names,
                   std::ostream &decl_os, std::ostream &os, std::string_view indent,
                   const std::unordered_map<std::string, bool> *assumptions = nullptr) const;
  void emit_expr_inline(const ExprGraph &expr_graph, ExprId id, std::string_view lhs,
                        std::ostream &os,
                        const std::unordered_map<std::string, bool> *assumptions = nullptr) const;
  bool lookup_assumed_condition(const ExprGraph &expr_graph, ExprId id,
                                const std::unordered_map<std::string, bool> *assumptions,
                                bool &value) const;
  bool can_emit_direct_range_base(const ExprGraph &expr_graph, ExprId id) const;
  void emit_shifted_range(const ExprGraph &expr_graph, ExprId data_id, ExprId base_id,
                          SignalWidth width, std::string_view lhs, std::ostream &os,
                          const std::unordered_map<std::string, bool> *assumptions = nullptr) const;
};

} // namespace abys::ir
