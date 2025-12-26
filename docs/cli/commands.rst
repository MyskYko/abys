Commands
========

This page will list CLI and interactive commands. Each command should have its
own section with syntax, arguments, and examples.

parse
-----

.. code-block:: text

   abys parse <files...> [--top <module>]

- **Purpose**: Parse SystemVerilog inputs with slang.
- **Inputs**: One or more SystemVerilog files.
- **Options**: `--top` selects the top module.
- **Output**: Reports success/failure and prepares the compilation for later passes.
- **Notes**: Requires slang installed and discoverable by CMake (set `slang_DIR` if needed).
