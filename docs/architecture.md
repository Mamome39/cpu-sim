# Architecture

## Design Philosophy

The simulator models **real hardware semantics**, not a functional step-through:

- Pipeline stages separate **combinational logic** (`evaluate()`) from **sequential elements** (`latch()`), matching RTL flip-flop behaviour.
- Each clock cycle: all stages evaluate in parallel, then all latch simultaneously. Stalls hold `latch()` from firing; flushes write a bubble instead.
- The ISA layer is **isolated and swappable** — `uarch/` and `memory/` are ISA-agnostic, so adding `isa/arm/` later leaves the pipeline intact.

## Two-phase clock protocol

Every component inherits from `Stage` and implements two methods:

| Method | Represents | What it may touch |
|--------|-----------|-------------------|
| `evaluate()` | Combinational logic | Reads latches, writes next-state shadow registers |
| `latch()` | Rising clock edge | Commits shadow → visible state; may suppress (stall) or overwrite with bubble (flush) |

All `evaluate()` calls happen before any `latch()` call, so stages see a
consistent snapshot of the pipeline from the previous cycle regardless of
the order they are called.

## File map

### ISA Layer (`include/cpusim/isa/rv32i/`)
| File | Role |
|------|------|
| `defs.h` | Opcodes, funct3/7 constants |
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
| `pipeline/writeback.h` | WB stage |
| `pipeline/forward.h` | Operand forwarding (3 paths, owns `prev_mem_wb`) |
| `hazard.h` | Load-use stall detection |

### Memory (`include/cpusim/memory/`)
| File | Role |
|------|------|
| `mem_interface.h` | `IMemory` — abstract interface all levels implement |
| `flat_mem.h` | Byte-addressable backing store (little-endian) |
| `cache.h` | Parameterised cache (sets/ways/line size) *(future)* |

### Simulation Infrastructure (`include/cpusim/sim/`)
| File | Role |
|------|------|
| `core.h` | Wires all stages + memory together, drives tick loop |
| `tracer.h` | Commit trace output for Spike diff-testing |
| `sim_config.h` | All tunable parameters (base, size, latencies) |
| `sim_stats.h` | Cycle counter, IPC, stall breakdown collector *(future)* |
