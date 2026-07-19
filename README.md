# cpu-sim

A cycle-accurate CPU + memory simulator built as a learning milestone in
computer architecture. Immediate target: **RV32I (RISC-V 32-bit integer)**
as a clean on-ramp to the ultimate goal of working with **ARM ISA**.

**Collaboration model:** Claude implements source code and tests. I review,
direct goals, and validate correctness.

---

## Documentation

- [Architecture](docs/architecture.md) — pipeline design, two-phase clock protocol, file map
- [Hazard Detection & Forwarding](docs/hazard-forwarding.md) — ForwardUnit, HazardUnit, load-use stall
- [Simulation](docs/simulation.md) — Core API, Tracer, Spike diff-testing workflow
- [Memory Model](docs/memory-model.md) — functional/timing split, ready()/tick(), cache, future revisions
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
| 4b | Performance mode — `SimConfig` latency knobs + `SimStats` report | Not started |
| 5 | L1 D-cache — direct-mapped, write-allocate, timing overlay | Done ✅ |
| 5b | L1 caches — I-cache, set-associative + second-chance, writeback penalty | Not started |
| 6 | L2 unified cache | Not started |
| 7 | Branch predictor | Not started |
| 8 | TLB + virtual memory | Not started |
| 9 | Tomasulo + ROB (out-of-order execution) | Not started |
| 10 | Fine-grained multithreading | Not started |
| 11 | OS support (context switch, syscall handling) | Not started |
| 12 | ARM ISA (`isa/arm/`) | Future |

### Performance mode (Stage 4b)

The simulator will support a **performance mode** alongside correctness mode:

**Correctness mode** — commit trace compared against Spike (`--log-commits`).
Validates register values, memory writes, and PC sequencing instruction by
instruction. (Done — see [Simulation](docs/simulation.md).)

**Performance mode** — counts cycles and reports where they were spent.
Component latencies are configured in `SimConfig` before the run (e.g.
L1 = 1 cycle, L2 = 10 cycles, DRAM = 100 cycles, TLB = 4 cycles).
`SimStats` accumulates stall causes each cycle — load-use, cache miss,
TLB miss, branch mispredict — and prints a summary with total cycles,
instructions retired, IPC, and per-cause stall counts at the end.
