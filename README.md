# cpu-sim

A side project to solidify computer architecture knowledge through implementation, with Claude as the primary coding contributor.

### Collaboration Model
Claude implements source code and tests. I review, direct goals, and validate correctness.

### Final Goal
A progressively more realistic processor model, built in stages:

| Stage | Feature |
|-------|---------|
| 1 | 5-stage in-order pipeline (IF → ID → EX → MEM → WB) |
| 2 | Data hazard detection + forwarding (hazard unit) |
| 3 | L1 I-cache + D-cache (write-back, write-allocate, second-chance eviction) |
| 4 | L2 unified cache |
| 5 | Branch predictor |
| 6 | TLB + virtual memory |
| 7 | Tomasulo algorithm + reorder buffer (out-of-order execution) |
| 8 | Fine-grained multithreading |
| 9 | OS support (context switch, syscall handling) |

### Microarchitecture Design
5-stage pipeline: IF → ID → EX → MEM → WB

Each stage follows a two-phase protocol modelling real flip-flop behaviour:
- `evaluate()` — combinational logic; computes results into a shadow register
- `latch()` — clocked commit; shadow → pipeline register (stall = skip, flush = clear)

### Design Decisions
- Branch outcome resolved in EX stage (2-cycle penalty on mispredict; no predictor yet)

### Progress
- [x] Base components: `RegFile`, `FlatMem`, `IMemory` interface
- [x] RV32I decode layer: constants, field extractors, instruction decoder
- [x] ALU + branch comparator
- [x] Latch\<T\> template
- [x] Pipeline stages: IF, ID, EX, MEM
- [ ] Pipeline stage: WB
- [ ] Hazard unit (load-use stall + forwarding)
- [ ] Full pipeline integration + simulation loop
- [ ] Spike diff-testing against commit trace
