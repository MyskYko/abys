#pragma once

#include <ostream>
#include <string>

#include "abys/ir/tig.h"

namespace abys::ir {

  class VerilogEmitter {
  public:
    explicit VerilogEmitter(const Tig &design);
    void emit(std::ostream& os) const;
    //std::string emit_to_string(const Design& design) const;
    
    using Module = Tig::Module;
    
  private:
    const Tig &design_;
    
    void emit_module(const Module &module, std::ostream &os) const;
    void emit_module_header(const Module &module, std::ostream &os) const;
    void emit_signal_decls(const Module &module, std::ostream &os) const;
    void emit_instances(const Module &module, std::ostream &os) const;
    void emit_combinational(const Module &module, std::ostream &os) const;
    void emit_sequential(const Module &module, std::ostream &os) const;
    void emit_output_binds(const Module &module, std::ostream &os) const;
    void emit_module_footer(std::ostream &os) const;

    void emit_expr(std::string_view lhs, bool nonblocking, const ExprGraph &expr_graph, ExprId id, std::ostream &os, std::string_view indent) const;
    void emit_expr_rec(const ExprGraph &expr_graph, ExprId id, std::string_view lhs, std::ostream &os) const;
  };

} // namespace abys::ir
