# Development

## Build

```bash
cmake -S . -B build
cmake --build build
```

Slang parsing is required. If slang is installed outside the default CMake search
path, set `slang_DIR` to its CMake package directory. CI uses a prebuilt slang
tarball and sets `slang_DIR` accordingly.

## Test

```bash
ctest --test-dir build
```

## Formatting

```bash
./scripts/format_check.sh
```

## Lint

```bash
./scripts/run_clang_tidy.sh build/compile_commands.json
```

## Docs

```bash
python3 -m venv .venv-docs
source .venv-docs/bin/activate
pip install -r docs/requirements.txt
make -C docs html
```

Note: Docs are uploaded as CI artifacts today; GitHub Pages publishing can be
added later.
