# cpu-sim

A cycle-accurate CPU + memory simulator built as a learning milestone in
computer architecture. Immediate target: **RV32I (RISC-V 32-bit integer)**
as a clean on-ramp to the ultimate goal of working with **ARM ISA**.

**Collaboration model:** Claude implements source code and tests. I review,
direct goals, and validate correctness.

---

## Documentation

- [Architecture](docs/architecture.md) — pipeline design, two-phase clock protocol, stalls, file map
- [Hazard Detection & Forwarding](docs/hazard-forwarding.md) — ForwardUnit, HazardUnit, load-use stall
- [Simulation](docs/simulation.md) — Core API, Tracer, Spike diff-testing workflow
- [Memory Model](docs/memory-model.md) — functional/timing split, ready()/tick(), D-/I-cache (associativity, write-back), future revisions
- [Branch Prediction](docs/branch-prediction.md) — BTB + bimodal, and the gshare upgrade path
- [Implementation Notes](docs/implementation-notes.md) — design decisions and the tradeoffs behind them
- [Performance Report](report.md) — cycles/IPC per memory configuration

---

## Quick Start

```sh
cmake -S . -B build
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

All tests should pass. To run a specific suite:

```sh
./build/core_test --gtest_filter=Core.*
./build/trace_test --gtest_filter=Trace.*
```

---

## Roadmap

| Stage | Feature | Status |
|-------|---------|--------|
| 1 | 5-stage in-order pipeline (IF → ID → EX → MEM → WB) | Done ✅ |
| 2 | Hazard unit — load-use stall + forwarding | Done ✅ |
| 3 | Full pipeline integration + simulation loop | Done ✅ |
| 4 | Spike diff-testing against commit trace | Done ✅ |
| 4b | Performance mode — `SimConfig` latency knobs + `SimStats` report | Done ✅ |
| 5 | L1 D-cache — direct-mapped, write-allocate, timing overlay | Done ✅ |
| 5b | L1 caches — set-associative (tree-PLRU) + write-back timing, I-cache with front-end stall | Done ✅ |
| 5c | Instruction prefetch (next-line + overlapped engine) | Not started |
| 6 | L2 unified cache | Not started |
| 7 | Branch predictor — BTB + bimodal (2-bit) | Done ✅ |
| 7b | gshare upgrade (global history) | Not started |
| 8 | TLB + virtual memory | Not started |
| 9 | Tomasulo + ROB (out-of-order execution) | Not started |
| 10 | Fine-grained multithreading | Not started |
| 11 | OS support (context switch, syscall handling) | Not started |
| 12 | ARM ISA (`isa/arm/`) | Future |

### Performance mode (Stage 4b)

The simulator supports a **performance mode** alongside correctness mode:

**Correctness mode** — commit trace compared against Spike (`--log-commits`).
Validates register values, memory writes, and PC sequencing instruction by
instruction. (See [Simulation](docs/simulation.md).)

**Performance mode** — counts cycles and reports where they were spent.
Component latencies and cache/predictor geometry are configured in
`SimConfig` before the run (memory latency, D-/I-cache size + ways,
BTB/BHT entries). `SimStats` is a passive probe attached to EX that
counts branch outcomes (correct vs. mispredicted); `bench_run` prints
cycles, IPC, and branch accuracy at the end. Stall-cause cycle counts
(load-use vs. cache-miss vs. fetch-stall) aren't broken out inside a
single run yet — today they're isolated by comparing separate runs
across configs, as in [Performance Report](report.md).
