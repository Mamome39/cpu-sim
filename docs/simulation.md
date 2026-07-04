# Simulation

## Core API

`Core` (`sim/core.h`) wires the full 5-stage pipeline and drives the clock.

### Setup

```cpp
Core core(0x80000000, 0x10000);    // base address, memory size
core.load_program({...});          // copy words into imem at base
core.write_reg(3, 0x80000000);    // pre-load register (test / ELF loader)
core.store_word(0x80000000, 42u); // pre-load dmem
```

### Running

```cpp
core.run();           // run until EBREAK retires (no cycle limit)
core.run(100'000);    // run until EBREAK or 100 000 cycles
core.tick();          // single clock step; returns false when halted
```

`max_cycles` defaults to `~0ULL` (effectively unlimited). It is a hard cap
on `Core::cycles_`, not a wall-clock timeout — useful for guarding against
infinite loops when no EBREAK is present.

### Inspection

```cpp
core.read_reg(1);     // register file read-back
core.load_word(addr); // dmem read-back
core.pc();            // current fetch PC
core.cycles();        // total clock cycles elapsed
core.halted();        // true once EBREAK has retired
core.read_regs();     // print all 32 registers to stdout (two-column)
```

### Known limitation — post-EBREAK pipeline drain

EBREAK halts the core when it reaches the WB input (`mem_wb_`). At that
point up to four instructions fetched speculatively after EBREAK are still
in flight. The cycle that detects EBREAK completes its latch phase normally,
so the instruction in MEM will have already issued its dmem write during
`mem.evaluate()`.

In practice this is harmless — well-formed programs do not place meaningful
instructions after EBREAK. A full fix would flush the pipeline when EBREAK
reaches EX (same mechanism as branch redirect), which is not yet implemented.

---

## Tracer

`Tracer` (`sim/tracer.h`) records one commit-log line per retired
instruction, in Spike's `--log-commits` format:

```
core   0: 3 0x80000000 (0x00100093) x1  0x00000001
core   0: 3 0x80000010 (0x0030a023) mem 0x80000000 0x0000002a
core   0: 3 0x80000018 (0x0000a283) x5  0x0000002a mem 0x80000000
core   0: 3 0x8000001c (0x00100073)
```

Fields: privilege level (always 3 = M-mode), PC, raw encoding, then:
- Register write: `x<N>  0x<val>` (same field-width alignment as Spike)
- Load: `x<N>  0x<val> mem 0x<addr>` (register write + load address)
- Store: `mem 0x<addr> 0x<val>` (no register write)
- No side effect (branches, EBREAK, NOPs): no trailing field

### Attaching

```cpp
std::ostringstream trace;
cpusim::Tracer t(trace);
core.set_tracer(&t);
core.run();
// trace.str() now holds the full commit log
```

The tracer pointer defaults to `nullptr`; tracing costs nothing when not
attached. Attach before the first `tick()` or `run()` call.

### Golden traces (`benchmarks/traces/`)

Pre-generated golden traces live in `benchmarks/traces/`. They document
expected Tracer output for small hand-assembled micro-tests and serve as
reference when regenerating or debugging the tracer.

---

## Benchmarking and Spike diff-testing

This section describes the full pipeline from C source to verified execution.

### Phase 1 — Compile

Benchmarks live in `benchmarks/`. Each benchmark is compiled as a bare-metal
RV32I ELF with no OS or standard library.

```
riscv64-unknown-elf-gcc \
    -march=rv32i -mabi=ilp32 -O1 \
    -nostdlib -nostartfiles \
    -T benchmarks/link.ld \
    benchmarks/start.S benchmarks/bubble_sort.c \
    -o build/bubble_sort.elf
```

**Key constraints:**

| Constraint | Reason |
|---|---|
| `-march=rv32i` | Restricts to the integer subset our decoder handles |
| `-nostdlib -nostartfiles` | No libc, no crt0 — `start.S` provides the entry |
| `-T benchmarks/link.ld` | Places `.text.init` at `0x80000000` (Core's base) |
| `start.S` | Sets sp from `_stack_top`, calls `main()`, writes `tohost`, `ebreak` |

`start.S` has a two-exit strategy:
- **Spike**: `sw t1, 0(tohost)` — Spike polls the `tohost` MMIO symbol and
  exits cleanly when it reads a non-zero value.
- **cpu-sim**: `ebreak` — our Core halts when EBREAK reaches the WB stage.

**CMake builds the benchmark automatically** when the RV32I toolchain is
found:
```
cmake --build build --target benchmarks
```

### Phase 2 — Load

`Core::load_elf(path)` reads the ELF file and loads all `PT_LOAD` segments
into both `imem_` and `dmem_`.

Harvard model workaround: our Core has separate instruction and data
memories, but ELF uses a unified address space. Loading into both ensures
that instruction fetches hit `imem_` and data accesses hit `dmem_` for the
same physical bytes.

BSS is zeroed in `dmem_` (`memsz − filesz` bytes). The entry point in the
ELF must match `Core::base_` (enforced by an exception); `link.ld` places
`_start` at exactly `0x80000000`.

### Phase 3 — Run

```bash
./build/bench_run [--trace=<file>] <elf> [max_cycles]
```

`bench_run` loads the ELF, optionally attaches a Tracer writing to
`<file>`, runs until EBREAK, then prints cycle count, wall time, and all
32 registers.

### Phase 4 — Spike diff

```bash
tools/spike_diff.sh <elf> [build_dir]
```

The script:

1. Locates `main`'s start address via `riscv64-unknown-elf-nm`.
2. Runs `bench_run --trace=...` to collect our commit log.
3. Runs `spike --isa=rv32i --log-commits` to collect Spike's commit log.
4. Filters both logs to instructions at PC ≥ `main` (excludes startup/exit
   glue so the diff is purely about the computation).
5. `diff`s the two filtered logs and prints `PASS` or `FAIL`.

Because our Tracer uses exactly Spike's `--log-commits` format — including
`x<N>` register names and load addresses — the diff is line-for-line with
no post-processing.

**Example output (bubble_sort, 16 645 instructions):**

```
main starts at 0x80000024
our trace:   16645 lines
spike trace: 16645 lines

PASS — traces match
```
