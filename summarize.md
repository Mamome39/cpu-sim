# cpu-sim — Project Summary

## Goal

A cycle-accurate CPU + memory simulator built as a learning milestone in computer architecture.
The immediate target is **RV32I (RISC-V 32-bit integer)**, chosen as a clean on-ramp to the
ultimate goal of working with **ARM ISA** (AArch32/AArch64).

---

## Design Philosophy

The simulator is not a functional step-through — it models **real hardware semantics**:

- Pipeline stages are separated into **combinational logic** (`evaluate()`) and
  **sequential elements** (`latch()`), matching how RTL flip-flops work.
- Each clock cycle: all stages evaluate in parallel, then all latch simultaneously.
  This makes stalls and flushes explicit (gate the `latch()` call).
- The ISA layer is **isolated and swappable** — `uarch/` and `memory/` are ISA-agnostic,
  so adding `isa/arm/` later leaves the core architecture intact.

---

## Planned Architecture

### ISA Layer (`include/cpusim/isa/rv32i/`)
| File | Role |
|------|------|
| `defs.h` | Opcodes, funct3/7, register aliases ✅ |
| `encoding.h` | Bit-field instruction layouts ✅ |
| `decoder.h` | Combinational decode logic ✅ |

### Microarchitecture (`include/cpusim/uarch/`)
| File | Role |
|------|------|
| `regfile.h` | 32 x 32-bit register file (x0 hardwired zero) ✅ |
| `alu.h` | Combinational ALU + branch comparator ✅ |
| `latch.h` | Pipeline register template — the core abstraction |
| `stage.h` | Base Stage interface: `evaluate()` + `latch()` |
| `pipeline/fetch.h` | IF stage |
| `pipeline/decode.h` | ID stage |
| `pipeline/execute.h` | EX stage |
| `pipeline/mem_access.h` | MEM stage |
| `pipeline/writeback.h` | WB stage |
| `hazard.h` | Hazard detection + forwarding unit |

### Memory Hierarchy (`include/cpusim/memory/`)
| File | Role |
|------|------|
| `mem_interface.h` | Abstract interface — all levels implement this |
| `flat_mem.h` | Backing byte-addressable store |
| `cache.h` | Generic parameterized cache (sets, ways, line size) |
| `l1.h` | L1 split I/D cache config |
| `l2.h` | L2 unified cache config |

### Simulation Infrastructure (`include/cpusim/sim/`)
| File | Role |
|------|------|
| `clock.h` | Drives `evaluate()` → `latch()` each tick |
| `core.h` | Wires all stages + memory hierarchy together |
| `tracer.h` | Execution trace / waveform output |

---

## Build Order

1. `isa/rv32i/` — decoder + instruction encoding (pure logic, testable in isolation)
2. `uarch/alu.h` — ALU (combinational, no state)
3. Single-stage loop to validate decode → execute end-to-end
4. Full pipeline: add stages one by one with explicit pipeline registers
5. `uarch/hazard.h` — data hazards + forwarding (most complex part)
6. Memory hierarchy: flat → L1 → L2, each adding a latency model
7. Multi-core: L2 becomes shared, add cache coherence protocol hooks

---

## Milestones

| Milestone | Status |
|-----------|--------|
| Register file (`RegFile`) | Done ✅ |
| ISA decoder | Done ✅ |
| ALU | Done ✅ |
| Pipeline latch abstraction | In progress 🔄 |
| 5-stage pipeline | Not started |
| Hazard / forwarding unit | Not started |
| L1 cache | Not started |
| L2 cache | Not started |
| Flat memory | Not started |
| Multi-core support | Future |
| ARM ISA (`isa/arm/`) | Future |

---

## Future Direction

Once the RV32I single-core is stable, the path to ARM is:
- Add `isa/arm/` (AArch32 first, then AArch64) with its own encoding and decoder
- The `uarch/` pipeline and `memory/` hierarchy require no structural changes
- Multi-core: promote L2 to a shared resource, implement a cache coherence protocol
  (MESI or similar) as a new `memory/coherence.h` layer
