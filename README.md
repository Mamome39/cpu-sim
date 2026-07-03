# cpu-sim

A cycle-accurate CPU + memory simulator built as a learning milestone in
computer architecture. Immediate target: **RV32I (RISC-V 32-bit integer)**
as a clean on-ramp to the ultimate goal of working with **ARM ISA**.

**Collaboration model:** Claude implements source code and tests. I review,
direct goals, and validate correctness.

---

## Documentation

- [Architecture](docs/architecture.md) — pipeline design, two-phase clock protocol, file map
- [Hazard Detection & Forwarding](docs/hazard-forwarding.md) — ForwardUnit, HazardUnit, load-use stall
- [Simulation](docs/simulation.md) — Core API, Tracer, Spike diff-testing workflow
- [Roadmap](docs/roadmap.md) — progress and future stages

---

## Quick Start

```sh
cmake -S . -B build
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

All tests should pass. To run a specific suite:

```sh
./build/core_test --gtest_filter=Core.*
./build/trace_test --gtest_filter=Trace.*
```
