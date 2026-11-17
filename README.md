# Project 09: Event Scheduler
## Team duckdsa — CS 253 / MA 253, Fall 2026

## Team and roles

| Name | Roll no. | GitHub username | Role | Module folder | Derivation |
|---|---:|---|---|---|---|
| Swarna Dhankhar | 250001070 | @dhankharswarna-dotcom | Structure A owner | `src/structA/` | D1 |
| Saumya Roy | 250001064 | @roysaumya767-code | Structure B owner | `src/structB/` | D2 |
| Jambula Pranav Reddy | 250001034 | @pranavreddy0188 | Simulation architect | `app/`, `src/` | D3 |
| Simran Malviya | 250001066 | @cse250001066-simi | Oracle and differential testing | `oracle/`, `tests/` | D4 |
| Henil Shah | 250001065 | @PeterParker38 | Adversarial inputs | `adversary/` | D5 |
| Piyushi Jagga | 250001055 | @piyushiijagga | Benchmark analysis | `benchmark/` | D6 |

## Summary

This project simulates a sender that schedules packet sends, acknowledgements, and retransmission timers in event-time order. It compares a binary min-heap with lazy cancellation against an AVL tree with immediate deletion, using a common `EventSet` interface. In the focused benchmark at 1,000 live events with exponential time increments, the heap had mean paired speedups of 4.13× at 0% cancellation and 2.23× at 95% cancellation. Those results cover two settings and five paired seeds each; they do not establish a general winner or a crossover rate.

## Requirements

- **Operating system:** Linux x86-64 was used for the recorded benchmark runs (kernel 6.18.57, glibc 2.41). The main application uses standard C++ and has no Linux-only dependency.
- **Compiler:** GCC 14.2.0, C++17, with `-O2 -Wall -Wextra -Wpedantic` for the recorded builds.
- **Libraries:** The C++ application and data structures use the standard library. Python 3 and `matplotlib` 3.5 or newer are needed to run the benchmark suite and regenerate figures. The exact Python version used for the stored results was not recorded.

## Build

Run the commands from the repository root. They build the simulator, tests, adversarial generators, and benchmark tools.

### macOS Terminal

Install the Xcode command-line tools if needed with `xcode-select --install`. Then install the Python plotting dependency in a local environment and compile:

The recorded benchmark results were collected on Linux x86-64 with GCC 14.2.0. macOS benchmark runs use Apple Clang and should be treated as a separate environment.

```sh
set -e
mkdir -p build/run_all
python3 -m venv build/run_all/venv
PYTHON=build/run_all/venv/bin/python3
"$PYTHON" -m pip install -r benchmark/requirements.txt
build_project() { clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude "$@"; }
build_benchmark() { clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Ibenchmark/src "$@"; }

build_project app/main.cpp src/simulator.cpp -o build/run_all/sim
build_project tests/test_suite.cpp src/simulator.cpp -o build/run_all/test_suite
build_project oracle/differential_test.cpp -o build/run_all/oracle_differential
build_project tests/differential_test.cpp src/simulator.cpp -o build/run_all/random_differential
build_project tests/d3_tests.cpp src/simulator.cpp -o build/run_all/d3_tests
build_project adversary/heap_adversary.cpp -o build/run_all/heap_adversary
build_project adversary/avl_adversary.cpp -o build/run_all/avl_adversary
build_benchmark benchmark/src/benchmark.cpp -o build/run_all/p6_bench
build_benchmark benchmark/tests/team_tests.cpp -o build/run_all/team_tests
build_benchmark benchmark/tests/fault_detection.cpp -o build/run_all/fault_detection
```

### Windows PowerShell

Install GCC (g++) and Python 3 first. Run these commands from the repository root:

```powershell
py -3 -m pip install -r benchmark/requirements.txt

$ErrorActionPreference = 'Stop'
$BuildDir = 'build/run_all'
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$Flags = @('-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Iinclude')
function Invoke-Gpp {
    param([string[]]$CompilerArgs)
    & g++ @CompilerArgs
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: g++ $CompilerArgs" }
}

Invoke-Gpp -CompilerArgs ($Flags + @('app/main.cpp', 'src/simulator.cpp', '-o', "$BuildDir/sim.exe"))
Invoke-Gpp -CompilerArgs ($Flags + @('tests/test_suite.cpp', 'src/simulator.cpp', '-o', "$BuildDir/test_suite.exe"))
Invoke-Gpp -CompilerArgs ($Flags + @('oracle/differential_test.cpp', '-o', "$BuildDir/oracle_differential.exe"))
Invoke-Gpp -CompilerArgs ($Flags + @('tests/differential_test.cpp', 'src/simulator.cpp', '-o', "$BuildDir/random_differential.exe"))
Invoke-Gpp -CompilerArgs ($Flags + @('tests/d3_tests.cpp', 'src/simulator.cpp', '-o', "$BuildDir/d3_tests.exe"))
Invoke-Gpp -CompilerArgs ($Flags + @('adversary/heap_adversary.cpp', '-o', "$BuildDir/heap_adversary.exe"))
Invoke-Gpp -CompilerArgs ($Flags + @('adversary/avl_adversary.cpp', '-o', "$BuildDir/avl_adversary.exe"))
$BenchmarkFlags = @('-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Ibenchmark/src')
Invoke-Gpp -CompilerArgs ($BenchmarkFlags + @('benchmark/src/benchmark.cpp', '-o', "$BuildDir/p6_bench.exe"))
Invoke-Gpp -CompilerArgs ($BenchmarkFlags + @('benchmark/tests/team_tests.cpp', '-o', "$BuildDir/team_tests.exe"))
Invoke-Gpp -CompilerArgs ($BenchmarkFlags + @('benchmark/tests/fault_detection.cpp', '-o', "$BuildDir/fault_detection.exe"))
```

## Quick start

After compiling, run a small seeded sender simulation with the heap in macOS Terminal:

```sh
./build/run_all/sim --structure heap --events 10 --seed 12345
```

In Windows PowerShell, use:

```powershell
& .\build\run_all\sim.exe --structure heap --events 10 --seed 12345
```

Expected output:

```text
processed=6 scheduled=17 cancelled=11 retransmissions=1 maxLive=10 lastTime=127
cancelled%=64.7
traceHash=7f9078a4372938a7
```

Change `--structure heap` to `--structure avl` to run the same simulation with the AVL tree. The simulator also accepts `--ack` (permille), `--timers`, `--rto`, `--maxdelay`, `--gap`, and `--retries`; see `app/main.cpp` for accepted ranges and defaults.

## Tests

The test commands run the focused invariant suite, the one-million-operation oracle differential test, the 600,000-operation cross-structure differential test, the D3 simulator checks, the benchmark self-test, adapter regression checks, and deliberate-corruption rejection. In macOS Terminal, run:

```sh
set -e
for test in test_suite oracle_differential random_differential d3_tests p6_bench team_tests fault_detection; do
    if [ "$test" = p6_bench ]; then
        "./build/run_all/$test" --self-test
    else
        "./build/run_all/$test"
    fi
done
```

In Windows PowerShell, run the full check set after building:

```powershell
foreach ($Test in @('test_suite', 'oracle_differential', 'random_differential', 'd3_tests', 'p6_bench', 'team_tests', 'fault_detection')) {
    if ($Test -eq 'p6_bench') {
        & ".\build\run_all\$Test.exe" --self-test
    } else {
        & ".\build\run_all\$Test.exe"
    }
    if ($LASTEXITCODE -ne 0) { throw "$Test failed with exit code $LASTEXITCODE" }
}
```

The simulation-equivalence check in `test_suite` uses seed `12345` and a 1,000,000-event limit. `oracle_differential` uses seed `20261113` and defaults to 1,000,000 generated operations; an optional first argument changes that count. The other differential test uses seed `2026` for 600,000 operations. A passing oracle differential run ends with:

```text
ALL DIFFERENTIAL TESTS PASSED: 100% agreement across Oracle, Heap, and AVL.
```

The focused suite reports `Test Suite Results: 83 / 83 tests passed`; it exits with status 0 when the counts match. The D3 checks end with `ALL TESTS PASSED`, and the 600,000-operation differential test reports `600000 random operations, mismatches: 0 -> ALL AGREE`. The adapter regression prints `PASS: both contracts, repeated/missing cancellation, duplicate IDs, empty queues, instance isolation, purge boundary, four rotation counters`; the fault-injection test prints `PASS: deliberately corrupted minimum rejected`.

## Evaluation interface (hidden tests)

The shared data-structure interface is `EventSet` in `include/event_set.hpp`: `insert(const Event&)`, `deleteMin(Event&)`, `cancel(const Event&)`, `successor(long long, Event&)`, and `size()`. Events are ordered by `(time, seq)`, and `successor(t, out)` is inclusive (`time >= t`). The public simulator command is:

```sh
./build/run_all/sim --structure heap --events 1000 --seed 12345
./build/run_all/sim --structure avl --events 1000 --seed 12345
```

PowerShell equivalents:

```powershell
& .\build\run_all\sim.exe --structure heap --events 1000 --seed 12345
& .\build\run_all\sim.exe --structure avl --events 1000 --seed 12345
```

The program prints processed, scheduled, cancelled, retransmission, maximum-live, and final-time counts, followed by the cancellation percentage and a 16-digit trace hash. Hidden tests can exercise the shared `EventSet` API; the public app is a simulator CLI rather than an operation-file runner.

## Benchmarks and figures

The commands below run both benchmark datasets and regenerate their analysis figures. They use 1,000 live events, exponential increments, 0% and 95% cancellation, 3,000,000 measured cycles, 10,000 warm-up cycles, and five paired seeds for the focused trials. The exploratory grid covers 720 pairs / 1,440 trials across three queue sizes, 12 cancellation fractions, four distributions, and five seeds.

In macOS Terminal, run:

```sh
set -e
RUN_DIR="build/run_all/results.$(date +%Y%m%d-%H%M%S)"
PYTHON=build/run_all/venv/bin/python3
mkdir -p "$RUN_DIR/adversary/results/raw" "$RUN_DIR/focused/r0" "$RUN_DIR/focused/r95"
ROOT="$PWD"
(
    cd "$RUN_DIR/adversary"
    "$ROOT/build/run_all/heap_adversary"
    "$ROOT/build/run_all/avl_adversary"
)
./build/run_all/p6_bench team 1000 3000000 10000 0 exponential 123456789 5 "$RUN_DIR/focused/r0"
./build/run_all/p6_bench team 1000 3000000 10000 0.95 exponential 123456789 5 "$RUN_DIR/focused/r95"
"$PYTHON" benchmark/analyze.py "$RUN_DIR/focused"

BUILD_DESCRIPTION="$(clang++ --version | head -n 1) -std=c++17 -O2 -Wall -Wextra -Wpedantic"
"$PYTHON" benchmark/run_suite.py --exe "$PWD/build/run_all/p6_bench" --mode team --out "$RUN_DIR/exploratory" --steps 5000 --warmup 2000 --repeats 5 --seed 123456789 --build-description "$BUILD_DESCRIPTION"
"$PYTHON" benchmark/analyze.py "$RUN_DIR/exploratory"
printf 'Generated benchmark data and figures: %s\n' "$RUN_DIR"
```

To run the benchmark stages from Windows PowerShell after building, create a fresh output folder and run:

```powershell
$RunDir = Join-Path 'build/run_all' ("results." + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$FocusedDir = Join-Path $RunDir 'focused'
$AdversaryDir = Join-Path $RunDir 'adversary'
New-Item -ItemType Directory -Force -Path (Join-Path $AdversaryDir 'results/raw'), "$FocusedDir\r0", "$FocusedDir\r95" | Out-Null
$HeapAdversary = (Resolve-Path '.\build\run_all\heap_adversary.exe').Path
$AvlAdversary = (Resolve-Path '.\build\run_all\avl_adversary.exe').Path
Push-Location $AdversaryDir
try {
    & $HeapAdversary
    if ($LASTEXITCODE -ne 0) { throw 'Heap adversary run failed' }
    & $AvlAdversary
    if ($LASTEXITCODE -ne 0) { throw 'AVL adversary run failed' }
} finally {
    Pop-Location
}

& .\build\run_all\p6_bench.exe team 1000 3000000 10000 0 exponential 123456789 5 "$FocusedDir\r0"
if ($LASTEXITCODE -ne 0) { throw 'Focused 0% cancellation benchmark failed' }
& .\build\run_all\p6_bench.exe team 1000 3000000 10000 0.95 exponential 123456789 5 "$FocusedDir\r95"
if ($LASTEXITCODE -ne 0) { throw 'Focused 95% cancellation benchmark failed' }
py -3 benchmark/analyze.py $FocusedDir
if ($LASTEXITCODE -ne 0) { throw 'Focused analysis failed' }

$ExploratoryDir = Join-Path $RunDir 'exploratory'
$CompilerVersion = (& g++ --version | Select-Object -First 1)
$BuildDescription = "$CompilerVersion -std=c++17 -O2 -Wall -Wextra -Wpedantic"
$BenchExe = (Resolve-Path '.\build\run_all\p6_bench.exe').Path
py -3 benchmark/run_suite.py --exe $BenchExe --mode team --out $ExploratoryDir --steps 5000 --warmup 2000 --repeats 5 --seed 123456789 --build-description $BuildDescription
if ($LASTEXITCODE -ne 0) { throw 'Exploratory benchmark run failed' }
py -3 benchmark/analyze.py $ExploratoryDir
if ($LASTEXITCODE -ne 0) { throw 'Exploratory analysis failed' }

Write-Output "Generated benchmark data and figures: $RunDir"
```

The exploratory trials each measured under 0.2 seconds; the full run takes several minutes depending on the machine. The focused dataset contains 20 measured trials. Diagnostics and validation are outside the timed interval. The benchmark figures and raw CSV files are written under the newly created `build/run_all/results.*` folder.

## Repository map

- `include/` — shared event, interface, RNG, and simulator headers.
- `src/` — primary heap and AVL structures, simulator implementation, and an earlier simulator entry point.
- `app/` — current sender-simulation command-line application.
- `oracle/` — sorted-array reference implementation, differential test, and shrinker.
- `tests/` — focused unit, regression, and simulator tests.
- `adversary/` — heap purge-policy and AVL rotation stress generators.
- `benchmark/` — integrated benchmark adapters, runner, analysis code, raw results, and figures.
- `results/raw/` — checked-in adversarial experiment CSV files.
- `proofs/` — D3 determinism proof notes.
- `docs/derivations/` — individual derivation PDFs.
- `scripts/` — build, test, demo, benchmark, and figure-generation workflow files.

## Results at a glance

Focused benchmark results use the integrated benchmark adapters, not the primary classes under `src/structA/` and `src/structB/`.

| Live size | Cancellation | Heap mean API calls/s | AVL mean API calls/s | Mean paired heap speedup (approx. 95% interval) |
|---:|---:|---:|---:|---:|
| 1,000 | 0% | 14,967,779 | 3,627,443 | 4.13× (3.78–4.48×) |
| 1,000 | 95% | 6,174,327 | 2,819,197 | 2.23× (1.94–2.53×) |

The measurements use five paired seeds per condition. The exploratory grid is coverage and preliminary pattern data, not a reliable performance ranking. See [REPORT.md](REPORT.md) for methods, limitations, and the full analysis.

## Known limitations

- The focused longer results cover one live size and two cancellation rates; machine CPU and cache details were not recorded.
- The benchmark compares integrated adapters, whose auxiliary indexes and event layouts differ, rather than isolated heap and AVL algorithms.
- A project-specific operation-file evaluator is not included; the documented public interface is the `EventSet` API and simulator CLI.

## Tools and references

- Tool declaration and representative prompts: [TOOLS.md](TOOLS.md).
- Main paper: Douglas W. Jones, “An Empirical Comparison of Priority-Queue and Event-Set Implementations,” *Communications of the ACM*, 29(4), 300–311, 1986. [DOI: 10.1145/5684.5686](https://doi.org/10.1145/5684.5686).
- Full bibliography and project materials: [REPORT.md](REPORT.md), section 13.
