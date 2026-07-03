# Roadmap

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

## Performance mode (Stage 4b)

The simulator will support a **performance mode** alongside correctness mode:

**Correctness mode** — commit trace compared against Spike (`--log-commits`).
Validates register values, memory writes, and PC sequencing instruction by
instruction.

**Performance mode** — counts cycles and reports where they were spent.
Component latencies are configured in `SimConfig` before the run (e.g.
L1 = 1 cycle, L2 = 10 cycles, DRAM = 100 cycles, TLB = 4 cycles).
`SimStats` accumulates stall causes each cycle — load-use, cache miss,
TLB miss, branch mispredict — and prints a summary with total cycles,
instructions retired, IPC, and per-cause stall counts at the end.
