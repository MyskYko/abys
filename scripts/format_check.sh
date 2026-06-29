#!/usr/bin/env bash
set -euo pipefail

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found" >&2
  exit 1
fi

mapfile -t files < <(git ls-files '*.h' '*.hpp' '*.cc' '*.cpp')
if [ ${#files[@]} -eq 0 ]; then
  echo "No source files to check."
  exit 0
fi

clang-format -n -Werror "${files[@]}"
