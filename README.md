# cpu-sim

A cycle-accurate CPU + memory simulator built as a learning milestone in computer architecture.
Immediate target: **RV32I (RISC-V 32-bit integer)** as a clean on-ramp to the ultimate goal of working with **ARM ISA**.

### Collaboration Model
Claude implements source code and tests. I review, direct goals, and validate correctness.

---

## Design Philosophy

The simulator models **real hardware semantics**, not a functional step-through:

- Pipeline stages separate **combinational logic** (`evaluate()`) from **sequential elements** (`latch()`), matching RTL flip-flop behaviour.
- Each clock cycle: all stages evaluate in parallel, then all latch simultaneously. Stalls skip `latch()`; flushes clear it.
- The ISA layer is **isolated and swappable** — `uarch/` and `memory/` are ISA-agnostic, so adding `isa/arm/` later leaves the core intact.

---

## Design Decisions

- Branch outcome resolved in EX stage (2-cycle penalty on mispredict; no predictor yet)

---

## Architecture

### ISA Layer (`include/cpusim/isa/rv32i/`)
| File | Role |
|------|------|
| `defs.h` | Opcodes, funct3/7, register aliases |
| `encoding.h` | Bit-field extractors and immediate decoders |
| `decoder.h` | Combinational decode — raw word → `Instruction` |

### Microarchitecture (`include/cpusim/uarch/`)
| File | Role |
|------|------|
| `regfile.h` | 32 × 32-bit register file (x0 hardwired zero) |
| `alu.h` | Combinational ALU + branch comparator |
| `latch.h` | Pipeline register template (`write` / `latch` / `flush`) |
| `stage.h` | Abstract base: `evaluate()` + `latch()` |
| `pipeline/pipe_regs.h` | IfId, IdEx, ExMem, MemWb structs |
| `pipeline/fetch.h` | IF stage |
| `pipeline/decode.h` | ID stage |
| `pipeline/execute.h` | EX stage |
| `pipeline/mem_access.h` | MEM stage |
| `pipeline/writeback.h` | WB stage *(pending)* |
| `hazard.h` | Hazard detection + forwarding unit *(pending)* |

### Memory (`include/cpusim/memory/`)
| File | Role |
|------|------|
| `mem_interface.h` | `IMemory` — abstract interface all levels implement |
| `flat_mem.h` | Byte-addressable backing store (little-endian) |
| `cache.h` | Parameterised cache (sets/ways/line size) *(future)* |

### Simulation Infrastructure (`include/cpusim/sim/`) *(future)*
| File | Role |
|------|------|
| `core.h` | Wires all stages + memory together, drives tick loop |
| `tracer.h` | Commit trace output for Spike diff-testing |
| `sim_config.h` | Per-component latency config (see below) |
| `sim_stats.h` | Cycle counter, IPC, stall breakdown collector |

---

## Performance Benchmarking

The simulator supports two output modes:

**Correctness mode** — commit trace compared against Spike (`--log-commits`). Validates register values, memory writes, and PC sequencing.

**Performance mode** — counts cycles and reports where they were spent. Component latencies are configured in `SimConfig` before the run (e.g. L1=1 cycle, L2=10 cycles, DRAM=100 cycles, TLB=4 cycles). `SimStats` accumulates stall causes each cycle — load-use, cache miss, TLB miss, branch mispredict — and prints a summary with total cycles, instructions retired, IPC, and per-cause stall counts at the end.

---

## Roadmap

| Stage | Feature | Status |
|-------|---------|--------|
| 1 | 5-stage in-order pipeline (IF → ID → EX → MEM → WB) | WB pending 🔄 |
| 2 | Hazard unit — load-use stall + forwarding | Not started |
| 3 | Full pipeline integration + simulation loop | Not started |
| 4 | Spike diff-testing against commit trace | Not started |
| 4b | Performance mode — `SimConfig` latency knobs + `SimStats` report | Not started |
| 5 | L1 I-cache + D-cache (write-back, write-allocate, second-chance) | Not started |
| 6 | L2 unified cache | Not started |
| 7 | Branch predictor | Not started |
| 8 | TLB + virtual memory | Not started |
| 9 | Tomasulo + ROB (out-of-order execution) | Not started |
| 10 | Fine-grained multithreading | Not started |
| 11 | OS support (context switch, syscall handling) | Not started |
| 12 | ARM ISA (`isa/arm/`) | Future |
