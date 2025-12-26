# Architecture (early draft)

abys will follow a simple pipeline:

1. **Parse** SystemVerilog using `slang` and extract AST symbols (driver-based).
2. **Lower** AST constructs into a small, explicit intermediate representation (IR).
3. **Synthesize** using ABC/mockturtle passes.
4. **Emit** mapped Verilog suitable for PnR.

This document will grow as the core IR is defined.
