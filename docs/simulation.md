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

Benchmarks live in `benchmarks/`, one C file each. Each is compiled as a
bare-metal RV32I ELF with no OS or standard library:

| Benchmark | Exercises |
|---|---|
| `bubble_sort` | Nested loops, signed compares, word load/store |
| `fibonacci` | Recursion: `jal`/`jalr`, stack spills of `ra` |
| `crc32` | Bit-ops (`srl`/`xor`/`andi`), byte loads, table lookup |
| `matmul` | `__mulsi3` (software multiply) + dense `lw`/`sw` |
| `sieve` | Branch-dense control flow, byte `sb`/`lbu` |

```
riscv64-unknown-elf-gcc \
    -march=rv32i -mabi=ilp32 -O1 \
    -nostdlib -nostartfiles \
    -T benchmarks/link.ld \
    benchmarks/start.S benchmarks/<name>.c \
    $(riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 \
        -print-libgcc-file-name) \
    -o build/<name>.elf
```

**Key constraints:**

| Constraint | Reason |
|---|---|
| `-march=rv32i` | Restricts to the integer subset our decoder handles |
| `-nostdlib -nostartfiles` | No libc, no crt0 — `start.S` provides the entry |
| `-T benchmarks/link.ld` | Places `.text.init` at `0x80000000` (Core's base) |
| `libgcc.a` | RV32I has no `mul`/`div`; supplies `__mulsi3`/`__divsi3` |
| `start.S` | Sets sp from `_stack_top`, calls `main()`, writes `tohost`, `ebreak` |

`start.S` has a two-exit strategy:
- **Spike**: `sw t1, 0(tohost)` — Spike polls the `tohost` MMIO symbol and
  exits cleanly when it reads a non-zero value.
- **cpu-sim**: `ebreak` — our Core halts when EBREAK reaches the WB stage.

**CMake builds every benchmark automatically** when the RV32I toolchain is
found. To add one, drop `benchmarks/<name>.c` and append `<name>` to the
`BENCHMARKS` list in `CMakeLists.txt`:
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

1. Runs `bench_run --trace=...` to collect our commit log.
2. Runs `spike --isa=rv32i --log-commits` to collect Spike's commit log.
3. Filters both logs to program-region commits (PC ≥ `base`, dropping Spike's
   MROM boot preamble) and strips the trailing `ebreak` — Spike exits on the
   `tohost` store one instruction before cpu-sim commits `ebreak`.
4. `diff`s the two filtered logs and prints `PASS` or `FAIL`.

Filtering by `base` rather than `main` is deliberate: with `-O1` the compiler
may place helper functions below `main`, and a `main`-relative filter would
silently skip them (fibonacci's `fib` sits below `main`).

Because our Tracer uses exactly Spike's `--log-commits` format — `x<N>`
register names, load addresses, and access-width store values — the diff is
line-for-line with no post-processing.

**Example output (bubble_sort):**

```
our trace:   16652 lines
spike trace: 16652 lines

PASS — traces match
```
