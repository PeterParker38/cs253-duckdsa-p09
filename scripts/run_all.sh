#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXX="${CXX:-g++}"
PYTHON="${PYTHON:-python3}"
for tool in "$CXX" "$PYTHON"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "$tool" >&2
    exit 1
  fi
done
if ! "$PYTHON" -c 'import matplotlib, sys; sys.exit(tuple(map(int, matplotlib.__version__.split(".")[:2])) < (3, 5))' >/dev/null 2>&1; then
  printf 'Python 3 with matplotlib >= 3.5 is required. Set PYTHON to that interpreter.\n' >&2
  exit 1
fi

BUILD_DIR="$ROOT/build/run_all"
mkdir -p "$BUILD_DIR"
RUN_DIR="$(mktemp -d "$BUILD_DIR/results.XXXXXX")"
CXXFLAGS=(-std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude)

native_path() {
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -w "$1"
  else
    printf '%s' "$1"
  fi
}

printf 'Building project executables with %s...\n' "$CXX"
"$CXX" "${CXXFLAGS[@]}" app/main.cpp src/simulator.cpp -o "$BUILD_DIR/sim"
"$CXX" "${CXXFLAGS[@]}" tests/test_suite.cpp src/simulator.cpp -o "$BUILD_DIR/test_suite"
"$CXX" "${CXXFLAGS[@]}" oracle/differential_test.cpp -o "$BUILD_DIR/oracle_differential"
"$CXX" "${CXXFLAGS[@]}" tests/differential_test.cpp src/simulator.cpp -o "$BUILD_DIR/random_differential"
"$CXX" "${CXXFLAGS[@]}" tests/d3_tests.cpp src/simulator.cpp -o "$BUILD_DIR/d3_tests"
"$CXX" "${CXXFLAGS[@]}" adversary/heap_adversary.cpp -o "$BUILD_DIR/heap_adversary"
"$CXX" "${CXXFLAGS[@]}" adversary/avl_adversary.cpp -o "$BUILD_DIR/avl_adversary"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Ibenchmark/src benchmark/src/benchmark.cpp -o "$BUILD_DIR/p6_bench"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Ibenchmark/src benchmark/tests/team_tests.cpp -o "$BUILD_DIR/team_tests"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Ibenchmark/src benchmark/tests/fault_detection.cpp -o "$BUILD_DIR/fault_detection"

printf '\nRunning invariant and differential checks...\n'
"$BUILD_DIR/test_suite"
"$BUILD_DIR/oracle_differential"
"$BUILD_DIR/random_differential"
"$BUILD_DIR/d3_tests"
"$BUILD_DIR/p6_bench" --self-test
"$BUILD_DIR/team_tests"
"$BUILD_DIR/fault_detection"

printf '\nChecking the README sample command...\n'
EXPECTED=$'processed=6 scheduled=17 cancelled=11 retransmissions=1 maxLive=10 lastTime=127\ncancelled%=64.7\ntraceHash=7f9078a4372938a7'
ACTUAL="$("$BUILD_DIR/sim" --structure heap --events 10 --seed 12345)"
if [[ "$ACTUAL" != "$EXPECTED" ]]; then
  printf 'README sample output did not match.\nExpected:\n%s\nActual:\n%s\n' "$EXPECTED" "$ACTUAL" >&2
  exit 1
fi
printf '%s\n' "$ACTUAL"
ACTUAL_AVL="$("$BUILD_DIR/sim" --structure avl --events 10 --seed 12345)"
if [[ "$ACTUAL_AVL" != "$EXPECTED" ]]; then
  printf 'AVL sample output did not match the heap output.\nExpected:\n%s\nActual:\n%s\n' "$EXPECTED" "$ACTUAL_AVL" >&2
  exit 1
fi
HEAP_EVAL="$("$BUILD_DIR/sim" --structure heap --events 1000 --seed 12345)"
AVL_EVAL="$("$BUILD_DIR/sim" --structure avl --events 1000 --seed 12345)"
if [[ "$HEAP_EVAL" != "$AVL_EVAL" ]]; then
  printf 'Heap and AVL evaluation examples produced different traces.\nHeap:\n%s\nAVL:\n%s\n' "$HEAP_EVAL" "$AVL_EVAL" >&2
  exit 1
fi
printf 'Heap/AVL quick-start and evaluation commands match.\n'

printf '\nRunning adversarial generators...\n'
mkdir -p "$RUN_DIR/adversary/results/raw"
(
  cd "$RUN_DIR/adversary"
  "$BUILD_DIR/heap_adversary"
  "$BUILD_DIR/avl_adversary"
)

printf '\nRunning focused benchmark trials and generating figures...\n'
mkdir -p "$RUN_DIR/focused"
"$BUILD_DIR/p6_bench" team 1000 3000000 10000 0 exponential 123456789 5 "$(native_path "$RUN_DIR/focused/r0")"
"$BUILD_DIR/p6_bench" team 1000 3000000 10000 0.95 exponential 123456789 5 "$(native_path "$RUN_DIR/focused/r95")"
"$PYTHON" benchmark/analyze.py "$(native_path "$RUN_DIR/focused")"

printf '\nRunning exploratory benchmark grid and generating figures...\n'
BUILD_DESCRIPTION="$("$CXX" --version | sed -n '1p') -std=c++17 -O2 -Wall -Wextra -Wpedantic"
"$PYTHON" benchmark/run_suite.py \
  --exe "$(native_path "$BUILD_DIR/p6_bench")" --mode team --out "$(native_path "$RUN_DIR/exploratory")" \
  --steps 5000 --warmup 2000 --repeats 5 --seed 123456789 \
  --build-description "$BUILD_DESCRIPTION"
"$PYTHON" benchmark/analyze.py "$(native_path "$RUN_DIR/exploratory")"

printf '\nALL RUN-ALL STEPS PASSED\nGenerated run data and figures: %s\nExecutables: %s\n' "$RUN_DIR" "$BUILD_DIR"
