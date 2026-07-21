# Branch Prediction

## Current: BTB + bimodal (2-bit)

`uarch/branch_predictor.h` pairs a tagged, direct-mapped **BTB**
(PC → target + is-conditional) with a PC-indexed table of **2-bit
saturating counters** (the BHT). Prediction happens in IF by PC alone:

- BTB miss → predict not-taken (fetch falls through).
- BTB hit → conditional direction comes from the counter (`>= 2` = taken),
  an unconditional `JAL` is always taken. Redirect fetch to the stored
  target on a predicted-taken, with no bubble.

EX resolves the branch, redirects on a **misprediction** (actual ≠
predicted, a 2-cycle flush), and trains the predictor. The BTB gates
everything, so a taken prediction always carries a valid, deterministic
target. `JALR` is never stored (register target isn't deterministic).

It is **bimodal**: each branch is predicted from *its own* history only.

---

## Upgrade: global-history (gshare)

### Why

Bimodal cannot see **correlation between branches** — cases where one
branch's outcome depends on how *recent, different* branches went:

```c
if (x)  a();          // branch 1
if (!x) b();          // branch 2 is the opposite of branch 1
```

A global predictor captures this by indexing the direction table with the
outcomes of the last *H* branches, not just the branch's PC.

### The change (McFarling's gshare)

- Add a **Global History Register (GHR)**: an *H*-bit shift register
  holding the last *H* conditional-branch outcomes (1 = taken).
- Replace the PC-indexed BHT with a **Pattern History Table (PHT)** of
  2-bit counters indexed by `(pc >> 2) XOR ghr`. The XOR mixes address
  with history, spreading aliasing while preserving correlation.
- The 2-bit counters and their saturating update rule are **unchanged** —
  only the *index* changes.

### What stays the same

- **BTB** — still PC-indexed (a branch's target is per-address, not
  history-dependent). The "BTB gates prediction" structure is unchanged,
  and `JALR` stays unpredicted.
- The 2-bit counter update policy, the IF-predict / EX-resolve timing, the
  2-cycle mispredict penalty, and the opt-in `SimConfig` switch.

### Structural additions (the real work)

1. **A GHR** in the predictor.
2. **Carry the predict-time index down the pipe.** This is the crux.
   `predict()` computes the PHT index from the GHR *at fetch time*; by the
   time the branch reaches EX, other branches may have shifted the GHR.
   `update()` must train the *same* counter `predict()` read, so the branch
   must carry its predict-time index (or a GHR snapshot) in `IfId → IdEx`.
   Bimodal needed none of this — its index was just PC, identical at both
   ends.
3. **GHR update policy:**
   - *v1 (simple):* shift the GHR at **resolve** (EX). No speculation
     recovery; the history is a few branches stale but always consistent.
     Easiest correct first cut.
   - *refinement:* update the GHR **speculatively at predict**, with a
     checkpoint/restore on misprediction — more accurate, needs recovery
     machinery.

### Config

- `bpred_kind` (`bimodal` | `gshare`) — select at runtime to A/B compare.
- `bpred_ghr_bits` — history length *H*.
- PHT size — `2^H`, or larger to cut aliasing.

### Testing

- Mispredicts are recorded by the **`SimStats` probe** at EX resolution
  (`bench_run` prints `mispred:`), so a selectable `bpred_kind` lets us
  diff bimodal vs gshare mispredict counts on the benchmarks directly.
- The count is an EX-resolve metric, so it includes one speculative jump
  the retirement stream never sees — the `j` guard right after EBREAK in
  start.S resolves in EX but the halt lands at WB first. Harmless and
  constant (+1 per run).

### Expectation on our workloads

Our benchmarks are loop-dominated and already well-predicted by bimodal
(low mispredict rates). gshare shines on **correlated, data-dependent**
branches, which small single-thread kernels have few of — so expect a
modest or negligible win here. The value is the *mechanism*; a workload
with correlated branches (or SPEC-class code) is what would show it. This
is exactly why we started with bimodal.
