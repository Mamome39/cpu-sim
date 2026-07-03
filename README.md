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

### Hazard Detection and Forwarding

**Forwarding (ForwardUnit)** resolves data hazards for three gaps:

| Gap | Source | Path |
|-----|--------|------|
| 1 instruction | `ex_mem.alu_out` | EX → EX |
| 2 instructions | `mem_wb.wb_val` | MEM → EX |
| 3 instructions | `prev_mem_wb` | WB → EX |

`ForwardUnit` is stateful: it owns `prev_mem_wb`, a snapshot of `mem_wb`
captured each `evaluate()` and committed in `latch()` before `mem.latch()`
overwrites it. The other two sources are live pipeline latch references.
`ExecuteStage` calls `fwd.resolve()` for both operands and the store data.

**Load-use stall (HazardUnit)** handles the one case forwarding cannot:
a load result needed by the very next instruction (data not available until
end of MEM). On detection the hazard unit stalls IF (PC frozen) and flushes
ID (bubble into id_ex), giving the load one extra cycle to reach MEM.

Detection reads `rs1`/`rs2` from `if_id.raw` at fixed bit positions
[19:15] and [24:20] — the same positions the decode unit reads in parallel.
RV32I places register indices at these fixed offsets in every instruction
format precisely so hazard detection can run without waiting for decode output,
matching real hardware behaviour.

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
| `tracer.h` | Commit trace output for Spike diff-testing *(future)* |
| `sim_config.h` | Per-component latency config *(future)* |
| `sim_stats.h` | Cycle counter, IPC, stall breakdown collector *(future)* |

---

## Simulation

`Core` (`sim/core.h`) wires the full pipeline and drives the clock loop.

### Setup

```cpp
Core core(0x80000000, 0x10000);        // base address, memory size
core.load_program({...});              // copy words into imem at base
core.write_reg(3, 0x80000000);        // pre-load register (test / loader)
core.store_word(0x80000000, 42u);     // pre-load dmem (test / loader)
```

### Running

```cpp
core.run();               // run until EBREAK retires (no cycle limit)
core.run(100'000);        // run until EBREAK or 100 000 cycles
core.tick();              // single clock step; returns false when halted
```

`max_cycles` defaults to `~0ULL` (effectively unlimited). It is a hard
cap on `Core::cycles_`, not a timeout — useful for tests and to guard
against infinite loops when no EBREAK is present.

### Inspection

```cpp
core.read_reg(1);         // register file read-back
core.load_word(addr);     // dmem read-back
core.pc();                // current fetch PC
core.cycles();            // total clock cycles elapsed
core.halted();            // true once EBREAK has retired
```

### Known Limitation — post-EBREAK pipeline drain

EBREAK halts the core when it reaches the WB input (`mem_wb_`). At that
point up to four instructions that were speculatively fetched after EBREAK
are still in flight (MEM → EX → ID → IF). The cycle that detects EBREAK
completes its latch phase normally, so the instruction sitting in MEM during
that cycle will have already issued its dmem write in `mem.evaluate()`.

In practice this is harmless: well-formed programs do not place meaningful
instructions after EBREAK. A full fix would flush the pipeline when EBREAK
reaches EX (same mechanism as branch redirect), which is not yet implemented.

---

## Spike Diff-Testing

`Tracer` (`sim/tracer.h`) records one commit-log line per retired instruction
in Spike's `--log-commits` format:

```
core   0: 3 0x80000000 (0x00100093) ra   0x00000001
core   0: 3 0x80000010 (0x0030a023) mem 0x80000000 0x0000002a
core   0: 3 0x80000018 (0x00100073)
```

Fields: privilege level (3 = M-mode), PC, raw encoding, and either a
register write (`<abi-name>  0x<val>`) or a memory write (`mem <addr> <val>`).
Instructions with rd = x0 and no memory write produce no trailing field.

### Attaching the tracer

```cpp
std::ostringstream trace;
cpusim::Tracer t(trace);
core.set_tracer(&t);
core.run();
// trace.str() now holds the full commit log
```

### Golden traces (`tests/traces/`)

Pre-generated golden files live in `tests/traces/`. The `trace_test`
integration suite compares the simulator's output against these files.
They were produced by reasoning through each program by hand and can be
regenerated at any time using Spike.

### Regenerating with Spike

1. Install Spike and the RV32I toolchain:
   ```
   brew install riscv-tools          # macOS, or build from source
   ```

2. Assemble a test program and link it to the same base address:
   ```
   riscv32-unknown-elf-as -march=rv32i -o prog.o prog.s
   riscv32-unknown-elf-ld -Ttext=0x80000000 -o prog prog.o
   ```

3. Run Spike to produce the golden trace:
   ```
   spike --isa=rv32i --log-commits -m0x80000000:0x10000 prog \
       2>tests/traces/<name>.trace
   ```

4. Trim any Spike preamble lines (privilege setup before your first
   instruction) so the trace starts at PC `0x80000000`.

5. Update the expected string in `trace_test.cpp` to match the new file.

---

## Performance Benchmarking

The simulator supports two output modes:

**Correctness mode** — commit trace compared against Spike (`--log-commits`). Validates register values, memory writes, and PC sequencing.

**Performance mode** — counts cycles and reports where they were spent. Component latencies are configured in `SimConfig` before the run (e.g. L1=1 cycle, L2=10 cycles, DRAM=100 cycles, TLB=4 cycles). `SimStats` accumulates stall causes each cycle — load-use, cache miss, TLB miss, branch mispredict — and prints a summary with total cycles, instructions retired, IPC, and per-cause stall counts at the end.

---

## Roadmap

| Stage | Feature | Status |
|-------|---------|--------|
| 1 | 5-stage in-order pipeline (IF → ID → EX → MEM → WB) | Done ✅ |
| 2 | Hazard unit — load-use stall + forwarding | Done ✅ |
| 3 | Full pipeline integration + simulation loop | Done ✅ |
| 4 | Spike diff-testing against commit trace | In progress 🔧 |
| 4b | Performance mode — `SimConfig` latency knobs + `SimStats` report | Not started |
| 5 | L1 I-cache + D-cache (write-back, write-allocate, second-chance) | Not started |
| 6 | L2 unified cache | Not started |
| 7 | Branch predictor | Not started |
| 8 | TLB + virtual memory | Not started |
| 9 | Tomasulo + ROB (out-of-order execution) | Not started |
| 10 | Fine-grained multithreading | Not started |
| 11 | OS support (context switch, syscall handling) | Not started |
| 12 | ARM ISA (`isa/arm/`) | Future |
