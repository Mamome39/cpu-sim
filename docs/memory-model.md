# Memory Model

How the simulator models memory data and memory *timing* — and where the
current design will need to change as the project grows.

---

## Functional / timing split

All bytes live in exactly **one** place: the backing `FlatMem`, sized to
the full `ram_size_bytes` and treated as DRAM (the last level). It is the
sole authority for **data**.

Every level above it — currently the L1 cache — is a **timing overlay**. It
decides how many cycles an access costs; it never decides what value an
access returns. `load_*` / `store_*` delegate straight to the backing store,
so data is always correct and immediate.

The payoff of this split is the correctness guarantee that has held since the
memory-latency work: **the Spike commit-trace diff stays valid under any
cache/latency configuration**, because the data path never changes. Turning
the cache on or changing latencies moves only cycle counts, never values.

The cost is that a **timing-model bug cannot corrupt data** — which also
means the Spike diff cannot *catch* one. See "no timing oracle" below.

---

## The `ready()` / `tick()` protocol

A multi-cycle memory has to live inside the pipeline's two-phase clock. Any
`IMemory` exposes two methods for that (default: single-cycle, always ready):

- **`ready(addr, is_write = false)`** — a query, asked in the **evaluate**
  (combinational) phase: "is an access to `addr` done this cycle?" The
  first call lazily *starts* the request (registers the address, begins
  the timer). Returns `true` the cycle data is available; while `false`
  the caller stalls and asks again next cycle. `is_write` marks a store
  so a write-back cache can dirty the installed/hit line; levels that
  don't model dirtiness ignore it.
- **`tick()`** — the advance, called in the **latch** (sequential) phase:
  move the internal timer/state machine forward one clock. On completion it
  releases the port so the next request can start.

It mirrors the pipeline itself — `ready()` reads state, `tick()` commits
progress — and it **nests**: a cache is a server to the MEM stage and a
client to its backing memory, so on a miss it drives `backing.ready()` /
`backing.tick()`. L1 → L2 → DRAM composes with the same two calls.

**One outstanding request (blocking).** The memory serves a single access at
a time; the pipeline stalls until it completes.

---

## L1 caches (current)

Set-associative, write-allocate, **timing-only** overlay (`memory/cache.h`,
class `Cache`). The same class backs both the data and instruction sides —
one instance as the D-cache (over `dram_`), one as the I-cache (over
`imem_`) — so associativity, replacement, and write-back all apply to
both for free.

- Stores only **tags** (valid + dirty + tag) — no data (data lives in the
  backing store). See the split above.
- **Associativity.** `sets` sets of `ways` lines each; `ways == 1` is
  plain direct-mapped. On a miss the victim is the first invalid way,
  else the replacement policy's choice.
- **Replacement — tree pseudo-LRU.** Victim selection among *resident*
  ways is a separate `EvictionPolicy` (`memory/eviction_policy.h`),
  consulted only once a set is full; `TreePlru` (`memory/tree_plru.h`)
  is the only implementation today, using `ways − 1` bits per set. The
  interface is the seam for a future `SimConfig`-selectable policy
  (mirroring bimodal vs. gshare in [Branch Prediction](branch-prediction.md)).
- **Banks — documented, not implemented.** An N-way cache can be
  organized as N banks (`bank = (addr >> offset_bits) & (banks - 1)`),
  letting independent same-cycle accesses to *different* banks proceed
  in parallel while same-bank accesses conflict and serialize. This has
  **zero timing effect** under the current one-outstanding-blocking-
  request protocol (see below) — an in-order core never issues two
  accesses in the same cycle, so bank conflicts are structurally
  impossible. Banking becomes meaningful, and worth actually modeling,
  once the memory interface goes non-blocking (see the MSHR row below).
- Configured via `SimConfig` `dcache_*` / `icache_*` fields (line size,
  sets, ways, hit latency); **off by default**.

### Write-back timing

Each line carries a **dirty** bit alongside valid + tag. A store hit is
served at `hit_latency` and only sets `dirty` — no backing traffic per
store. The write-back cost is paid later, on eviction:

| Access | Victim | Cost |
|--------|--------|------|
| hit (load or store) | — | `hit_latency` |
| miss | clean or invalid | `backing_latency + hit_latency` |
| miss | dirty | `backing_latency` (writeback) `+ backing_latency` (fill) `+ hit_latency` |

A dirty-victim miss runs a **WriteBack** phase before **MissFetch**: it
drives the backing for the victim's *own* address (charging
`backing_latency`, i.e. `dmem_latency_cycles` for the D-cache) before
fetching the new line. There is no separate `writeback_latency` knob —
under [Option A](#functional--timing-split) the victim's data is already
correct in the backing store, so the flush is timing-only, driving
`ready()`/`tick()` and discarding the result; the flush also passes
`is_write=true` downward so a future cache backing (L2) dirties its own
line correctly. On install after a write-allocate store miss the new
line starts `dirty`; after a load miss it starts clean.

The **I-cache is read-only** — instructions are never stored, so its
lines are never dirty and it never pays a writeback. The dirty/write-back
machinery lives in the shared `Cache` class but stays dormant for the
I-cache instance.

### L1 instruction cache and the front-end stall

Fetch is a `ready()`/`tick()` client of its `IMemory` port, exactly like
MEM (`icache_ ? *icache_ : imem_`, selected the same way `dmem_` picks
`dcache_` vs. `dram_`). An I-miss stalls the **front end**, not the whole
pipe — see [Architecture § Stalls](architecture.md#stalls) for how that
differs from the MEM stall and why `Core::tick()` needed no new branch
for it.

The I-cache is deliberately **scalar** (one instruction per cycle), not
superscalar. That is a fetch-*bandwidth* question, orthogonal to the
fetch-*latency* question the cache answers, and our 1-wide in-order
issuer already runs at IPC ≈ 0.7 — stall-limited, not fetch-bandwidth-
limited — so widening fetch would feed a back end that can't drain it.
Spatial locality does the real work at 1-wide: one miss fills a whole
line (32 B = 8 instructions), and the next 7 sequential fetches hit.

**Known v1 pessimism:** the MEM stall dominates the fetch stall (an
in-flight I-miss timer pauses during a concurrent D-cache stall), even
though `imem_` and `dram_` are separate (Harvard) backings that a real
machine could service in parallel. The blocking, one-outstanding-request
protocol serializes them instead. Safe for an in-order core; revisit
together with the MSHR row below.

v1 omission (planned refinement, not a limitation of the model):
- **Instruction prefetch** — the I-cache above is demand-fetch only.
  Sequential prefetch (fill the next line ahead of the demand miss) is
  the next step; see [report.md](../report.md) once it lands.

---

## Known limitations → future revisions

These are seams in the *model*, not bugs. Each is marked with when it must be
revisited. None block the current single-core, in-order roadmap
(caches → predictor → TLB).

| Area | Current model | Revisit when | What changes |
|------|---------------|--------------|--------------|
| **Non-blocking / MSHRs** | `ready()`/`tick()` serves one blocking request | **Out-of-order execution** | The blocking two-call protocol is all an in-order core can use. Non-blocking caches (hit-under-miss, miss-under-miss, memory-level parallelism) need a **request/response interface** — `accept(Request{addr,id})` / `take_done()` — and an **MSHR** file tracking outstanding line fills and their waiters. Same-line misses coalesce into one MSHR. |
| **Multicore coherence** | Tags-only; data lives solely in DRAM | **Multicore** | A tags-only model can't express "the freshest copy is in core 2's L1." Cache-to-cache transfer timing depends on where data is, so we'd add **per-line coherence state** (MESI) and possibly real data back into the cache. |
| **Speculation** | Stores write the backing store immediately in MEM | **Branch speculation / OoO** | Immediate writes are safe only because in-order MEM is post-commit. A squashed speculative store must not touch memory — needs a **store queue** that holds writes until commit, then drains. |
| **No timing oracle** | Spike diff validates data; nothing validates cycles | **Ongoing** | Because the split guarantees timing bugs never corrupt data, they are invisible to the Spike diff and show up only as wrong cycle counts. Timing correctness relies on hand-written unit tests; a reference-model cross-check (gem5-class) would be the stronger answer. |

The recurring theme: the memory interface and the tags-only cache are exactly
right for a **single-core, in-order** machine, and they get upgraded — not
discarded — when the pipeline goes **out-of-order** and **multicore**. The
blocking cache remains the L2/DRAM backing behavior and the baseline the
non-blocking version is measured against.
