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
core   0: 3 0x80000000 (0x00100093) ra   0x00000001
core   0: 3 0x80000010 (0x0030a023) mem 0x80000000 0x0000002a
core   0: 3 0x80000018 (0x00100073)
```

Fields: privilege level (always 3 = M-mode), PC, raw encoding, and either
a register write (`<abi-name>  0x<val>`) or a memory write
(`mem <addr> <val>`). Instructions with rd = x0 and no memory side-effect
produce no trailing field (branches, EBREAK, NOPs).

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

### Golden traces (`tests/traces/`)

Pre-generated golden files live in `tests/traces/`. The `trace_test`
integration suite compares the simulator's output against these files.

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
