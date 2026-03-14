# CellWright

Cross-platform quantitative spreadsheet built with C++20 and Dear ImGui. Designed for financial and scientific workflows: columnar storage, formula engine with dependency tracking, SIMD aggregation, embedded DuckDB SQL, plugin system, and charting via ImPlot.

## Features

**Core Engine**
- Columnar storage with dense `vector<double>` and sparse non-numeric overlay
- Recursive-descent formula parser with arena-allocated AST
- 50+ built-in functions (math, statistics, text, logic, date)
- Dependency graph with topological recalculation and cycle detection
- Undo/redo (command pattern, 200 depth)
- JSON (`.magic`) and CSV serialization

**SIMD Aggregation**
- ARM NEON / AVX2+FMA / scalar fallback
- NaN-aware `sum`, `min`, `max`, `count`, `sum_of_squares`
- Fast path in evaluator for single-column SUM/MIN/MAX/COUNT

**Embedded SQL**
- DuckDB v1.1.3 in-process engine (lazy init, pimpl)
- `=SQL("SELECT ...")` formula function
- Interactive SQL editor panel
- Generation-based skip of redundant imports

**Plugin System**
- Three conventions: C++ (IFunction, hot-reloadable), C ABI (language-agnostic), Python (pybind11)
- SHA-256 trust store with codesign verification (macOS)
- Hot reload with mtime polling
- Panel plugins (custom ImGui windows)

**Arrow C Data Interface**
- Zero-copy export of sheet data to Arrow struct arrays
- Import Arrow record batches into sheets
- Interop with PyArrow, Polars, and pandas

**UI**
- Spreadsheet grid with freeze panes, drag-to-move, fill handle, range selection
- Formula bar with autocomplete, signature tooltips, reference highlighting
- Line / bar / scatter / histogram / stacked bar charts via ImPlot
- Find & replace, conditional formatting, per-column filtering
- Dark / light theme, DPI awareness
- Toast notifications, unsaved-changes guard, auto-save recovery
- Native file dialogs, drag-and-drop plugin loading

## Technology Stack

| Layer | Choice |
|-------|--------|
| Language | C++20 |
| UI | Dear ImGui (docking) + GLFW 3.4 + OpenGL 3 |
| Charting | ImPlot |
| Plugin framework | [cpp_plugin_arch](https://github.com/bdcbqa314159/cpp_plugin_arch) |
| Python plugins | pybind11 v2.13.6 |
| SQL engine | DuckDB v1.1.3 |
| SIMD | ARM NEON / AVX2+FMA / scalar |
| Interop | Arrow C Data Interface |
| File dialogs | nativefiledialog-extended v1.2.1 |
| Tests | Google Test v1.15.2 + pytest |
| Build | CMake 3.20+ |

## Build

```bash
# Configure (fetches all dependencies via FetchContent)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Run
./build/bin/cellwright
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `CELLWRIGHT_BUILD_PLUGINS` | `ON` | Build bundled plugins |
| `CELLWRIGHT_BUILD_TESTS` | `ON` | Build test suite |

### Platform dependencies

**macOS** — Xcode command line tools (ships with OpenGL, Cocoa frameworks)

**Linux** — install before building:
```bash
sudo apt-get install libgl-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxi-dev libgtk-3-dev python3-dev
```

**Windows** — Visual Studio 2022 with C++ workload

## Tests

```bash
# C++ tests (254 tests across 34 suites)
ctest --test-dir build --output-on-failure

# Python tests (Arrow interop, smoke tests)
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
pytest tests/python -v
```

## Bundled Plugins

| Plugin | Type | Functions |
|--------|------|-----------|
| `example_stats` | C++ IFunction | `STDEV_P` |
| `scientific` | C++ IFunction | Trig, log, statistical |
| `bond` | C ABI | `BOND_PRICE`, `BOND_YIELD`, `BOND_DURATION`, `BOND_CONVEXITY` |
| `py_bond` | Python | Bond pricing via pybind11 |

## License

[MIT](LICENSE) -- Bernardo Cohen / deLaPatada Software
