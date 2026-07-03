# Hazard Detection and Forwarding

## Branch resolution

Branch outcome is resolved in the EX stage. On a taken branch, EX sets a
redirect flag and flushes the IF and ID stages (two instructions squashed),
giving a **2-cycle mispredict penalty**. No branch predictor is implemented
yet — the pipeline always fetches sequentially.

## Forwarding (ForwardUnit)

`ForwardUnit` resolves RAW data hazards for three producer-consumer gaps:

| Gap | Source latch | Path |
|-----|-------------|------|
| 1 instruction | `ex_mem.alu_out` | EX → EX |
| 2 instructions | `mem_wb.wb_val` | MEM → EX |
| 3 instructions | `prev_mem_wb.wb_val` | WB → EX |

`ExecuteStage` calls `fwd.resolve(reg, stale_val)` for both source operands
and for store data before computing the ALU result.

### Why ForwardUnit is stateful

The EX→EX and MEM→EX paths read live latch references (`ex_mem_` and
`mem_wb_`), which are always current. The WB→EX path needs the value that
was in `mem_wb` *one cycle ago* — after `mem.latch()` has already overwritten
it. `ForwardUnit` solves this by snapshotting `mem_wb` during `evaluate()`
into `prev_mem_wb_next_`, then committing it to `prev_mem_wb_` in `latch()`
**before** `MemAccessStage::latch()` runs. This ordering is enforced in
`Core::tick()`:

```
wb.latch() → fwd.latch() → ex.latch() → mem.latch() → hazard.latch() → …
```

## Load-use stall (HazardUnit)

Forwarding cannot help when a load result is consumed by the immediately
following instruction — the data does not exist until the end of the MEM
stage, one cycle after EX needs it.

`HazardUnit` detects this pattern — a load in `id_ex` whose `rd` matches
`rs1` or `rs2` of the instruction currently in `if_id` — and responds by:

1. **Stalling IF** — PC is held, so the same instruction is re-fetched next cycle.
2. **Flushing ID** — a bubble is injected into `id_ex`, giving the load one extra cycle to reach MEM before the consumer enters EX.

After the stall cycle, the load result is in `mem_wb` and the MEM→EX
forwarding path delivers it to the consumer.

### Fixed bit positions — real hardware behaviour

Hazard detection reads `rs1` and `rs2` from `if_id.raw` at bit positions
[19:15] and [24:20] directly, without waiting for the decode stage to
extract them. RV32I places register indices at these fixed offsets in every
instruction format for exactly this reason — hazard detection and register
file reads can begin in parallel with decode, matching what real
implementations do.
