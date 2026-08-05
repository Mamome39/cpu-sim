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

## Stalls

Two independent stalls can hold `latch()` back; they are mutually
exclusive per cycle.

**Backward (MEM) stall** — a data-memory access not yet served
(`mem_.stalling()`) freezes upstream (IF/ID/EX) while downstream (WB)
still drains, since the instruction ahead of MEM has already finished
and retires regardless (`Core::tick()`). Correct forwarding across the
freeze is preserved by **operand capture**
(`Core::capture_ex_operands()`): while the producers behind the frozen
EX instruction are still in the forwarding window, its operands are
resolved and written back into `id_ex`, so later stall cycles read the
captured values once the real producers have drained past.

**Front-end (fetch) stall** — an instruction-cache miss
(`FetchStage::stalling()`) is the opposite direction: IF/ID takes a
bubble and PC holds, while everything downstream (ID/EX/MEM/WB)
continues to drain normally. This needs no special case in
`Core::tick()` — the existing full-latch path already calls
`fetch_.latch()` every non-mem-stall cycle, and the hold-PC/bubble logic
lives entirely inside `FetchStage::latch()`, gated on its own
`fetch_stall_` flag. A concurrent MEM stall always dominates (it already
skips `fetch_.latch()` entirely), so the two stalls never need to be
reasoned about together.

Both are pure timing effects — see the functional/timing split in
[Memory Model](memory-model.md).

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
| `branch_predictor.h` | BTB + bimodal (2-bit) predictor, predicts in IF, trained/resolved in EX |

### Memory (`include/cpusim/memory/`)
| File | Role |
|------|------|
| `mem_interface.h` | `IMemory` — abstract interface all levels implement |
| `flat_mem.h` | Byte-addressable backing store (little-endian) |
| `cache.h` | Set-associative, write-back-timing L1 overlay (`Cache`); backs both the D-cache and I-cache instances |
| `eviction_policy.h` | Replacement-policy interface, consulted only when a set is full |
| `tree_plru.h` | Tree pseudo-LRU `EvictionPolicy` implementation |

### Simulation Infrastructure (`include/cpusim/sim/`)
| File | Role |
|------|------|
| `core.h` | Wires all stages + memory together, drives tick loop |
| `tracer.h` | Commit trace output for Spike diff-testing |
| `elf_loader.h` | Loads `PT_LOAD` ELF segments into `imem_`/`dmem_` |
| `sim_config.h` | All tunable parameters (base, size, latencies, cache/predictor geometry) |
| `sim_stats.h` | Passive probe attached to EX; counts branch correct/mispredict |
