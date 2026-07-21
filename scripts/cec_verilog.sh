#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
abys_bin="$root_dir/build/abys"

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 ORIGINAL_VERILOG [TOP_MODULE]" >&2
  exit 2
fi

orig_v="$1"
top="${2:-}"

if [[ ! -x "$abys_bin" ]]; then
  echo "error: abys binary not found or not executable: $abys_bin" >&2
  exit 2
fi

if [[ ! -f "$orig_v" ]]; then
  echo "error: original Verilog file not found: $orig_v" >&2
  exit 2
fi
orig_v="$(realpath "$orig_v")"

yosys_bin="${YOSYS:-yosys}"

tmp_dir="$(mktemp -d /tmp/abys-cec-slang.XXXXXX)"
lowered_v="$tmp_dir/lowered.v"
equiv_ys="$tmp_dir/equiv.ys"
gold_v="$tmp_dir/gold.v"
gate_v="$tmp_dir/gate.v"
equiv_v="$tmp_dir/equiv.v"
emit_log="$tmp_dir/abys_emit.log"
equiv_log="$tmp_dir/yosys_equiv.log"
orig_yosys_v="$tmp_dir/original_no_stop.v"

normalize_passes="$(cat <<'EOF'
proc
flatten
opt
memory
opt
techmap
opt
delete t:$print
EOF
)"

sed 's/\$stop[[:space:]]*;//g' "$orig_v" >"$orig_yosys_v"

if [[ -n "$top" ]]; then
  read_orig_cmd="read_slang --ignore-timing --ignore-assertions --top $top $orig_yosys_v"
  read_gate_cmd="read_slang --ignore-timing --ignore-assertions --top $top $lowered_v"
else
  read_orig_cmd="read_slang --ignore-timing --ignore-assertions $orig_yosys_v"
  read_gate_cmd="read_slang --ignore-timing --ignore-assertions $lowered_v"
fi

cat >"$equiv_ys" <<EOF
$read_orig_cmd
$normalize_passes
rename -top gold
design -stash gold
design -reset

$read_gate_cmd
$normalize_passes
rename -top gate
design -stash gate
design -reset

design -copy-from gold gold
design -copy-from gate gate
select gold
write_verilog -selected "$gold_v"
select gate
write_verilog -selected "$gate_v"
equiv_make gold gate equiv
select equiv
async2sync
opt
equiv_simple
equiv_induct
select equiv
write_verilog -selected "$equiv_v"
equiv_status -assert equiv
EOF

echo "tmp: $tmp_dir"

if ! "$abys_bin" emit "$orig_v" >"$lowered_v" 2>"$emit_log"; then
  echo "fail: abys emit failed; log: $emit_log" >&2
  exit 1
fi

if ! "$yosys_bin" -m slang -q "$equiv_ys" >"$equiv_log" 2>&1; then
  echo "fail: Yosys/slang equivalence failed; log: $equiv_log" >&2
  exit 1
fi

echo "ok: Yosys/slang equivalence passed"
