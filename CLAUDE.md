# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A cycle-accurate CPU + memory simulator, targeting RV32I (RISC-V 32-bit
integer) as an on-ramp to eventually modeling ARM. Collaboration model:
Claude implements source code and tests; the user reviews, directs
goals, and validates correctness.

## Git workflow

- Commit every change (following the repo's normal commit conventions).
- **Never `git push` without asking first** — after committing, ask
  the user whether to push, every time, even if they approved a push
  earlier in the session.

## Commands

```sh
cmake -S . -B build
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

Run a single test suite or filter:
```sh
./build/core_test --gtest_filter=Core.*
./build/trace_test --gtest_filter=Trace.*
```

Build/run one test target directly:
```sh
cmake --build build --target <test_name>
./build/<test_name> --gtest_filter=Suite.Test
```

Clean rebuild: `rm -rf build/`

Run a benchmark through the simulator (perf-mode knobs are all optional):
```sh
./build/bench_run [--trace=<file>] [--cycles=<file>] [--mem-latency=<n>] \
    [--dcache] [--dcache-ways=<n>] [--imem-latency=<n>] [--icache] \
    [--icache-ways=<n>] [--bpred] <elf> [max_cycles]
```

Diff-test against Spike (requires `spike` and a built `bench_run`):
```sh
tools/spike_diff.sh <elf> [build_dir]
```

Run the vendored rv32ui ISA conformance suite (same requirements, plus
`riscv64-unknown-elf-gcc`). Also runs under `ctest` as the `rv32ui`
test when those tools are present:
```sh
tools/run_isa_tests.sh                    # whole suite
tools/run_isa_tests.sh build add sll      # named tests only
```

Benchmarks (`benchmarks/*.c`) only build if `riscv64-unknown-elf-gcc`
is on `PATH`; CMake silently skips the `benchmarks` target otherwise.

All library targets build with `-Wall -Wextra -Werror` — warnings are
build failures.

## Architecture

The simulator models real hardware semantics, not a functional
step-through. Read [docs/architecture.md](docs/architecture.md) first — it covers the
two-phase clock protocol and stall handling that everything else
depends on. In short:

- Every pipeline stage inherits `Stage` and splits into `evaluate()`
  (combinational — reads latches, writes shadow next-state) and
  `latch()` (rising edge — commits shadow to visible state, or
  suppresses/overwrites it for a stall/flush). All `evaluate()` calls
  across all stages happen before any `latch()` call each cycle, so
  every stage sees a consistent snapshot regardless of call order.
- Two mutually-exclusive stalls per cycle: a **backward (MEM) stall**
  (data-memory access not yet served) freezes IF/ID/EX while WB still
  drains, relying on operand capture (`Core::capture_ex_operands()`)
  to preserve forwarding across the freeze; a **front-end (fetch)
  stall** (I-cache miss) holds PC and bubbles IF/ID while everything
  downstream drains normally. A concurrent MEM stall always dominates.
- The ISA layer (`isa/rv32i/`) is isolated from `uarch/` and `memory/`,
  which are ISA-agnostic — this is what leaves room for `isa/arm/`
  later without touching the pipeline.

Layout:
- `include/cpusim/isa/rv32i/` + `src/isa/rv32i/` — opcodes, encoding,
  the combinational decoder.
- `include/cpusim/uarch/` + `src/uarch/` — regfile, ALU, pipeline
  registers (`latch.h`), the five pipeline stages
  (`uarch/pipeline/*`), operand forwarding, hazard/stall detection,
  branch predictor (BTB + bimodal).
- `include/cpusim/memory/` + `src/memory/` — `IMemory` interface,
  `FlatMem` backing store, `Cache` (set-associative, write-back L1,
  backs both D-cache and I-cache instances), tree-PLRU eviction.
- `include/cpusim/sim/` + `src/sim/` — `Core` (wires everything
  together, drives the tick loop), `Tracer` (Spike-compatible commit
  trace), `ElfLoader`, `SimConfig` (every tunable knob), `SimStats`
  (branch correct/mispredict counter).
- `tests/unit/` — one file per component; `tests/integration/` — full
  `Core`/pipeline/trace tests; `tests/isa/` — the official riscv-tests
  rv32ui suite, vendored. Everything outside `tests/isa/env/` is
  byte-identical to upstream and must not be edited; `env/` is our
  CSR-free replacement for riscv-test-env. See
  [tests/isa/README.md](tests/isa/README.md).
- `benchmarks/` — bare-metal RV32I C programs (`-nostdlib
  -nostartfiles`, linked at `0x80000000` via `benchmarks/link.ld`,
  entered through `benchmarks/start.S`) used for Spike diff-testing
  and the performance report.
- `tools/spike_diff.sh` — commit-trace diff against Spike;
  `tools/run_isa_tests.sh` — builds and runs the rv32ui suite through
  that diff; `tools/bench_run.cpp` — the benchmark/perf-mode CLI driver.

Two operating modes, both driven by `Core`/`SimConfig` (see
[docs/simulation.md](docs/simulation.md)):
- **Correctness mode** — commit trace compared line-for-line against
  Spike's `--log-commits` output via `tools/spike_diff.sh`.
- **Performance mode** — `SimConfig` sets latency/cache/predictor
  geometry before the run; `SimStats` and `bench_run` report cycles,
  IPC-relevant stats, and branch accuracy.

Further docs, read as needed for the area you're touching:
- [docs/hazard-forwarding.md](docs/hazard-forwarding.md) — ForwardUnit, HazardUnit, load-use stall
- [docs/memory-model.md](docs/memory-model.md) — functional/timing split, ready()/tick(), D-/I-cache
- [docs/branch-prediction.md](docs/branch-prediction.md) — BTB + bimodal, gshare upgrade path
- [docs/implementation-notes.md](docs/implementation-notes.md) — design decisions and tradeoffs
- [report.md](report.md) — cycles/IPC per memory configuration
