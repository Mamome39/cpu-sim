#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "cpusim/memory/cache.h"
#include "cpusim/memory/mem_interface.h"

namespace cpusim {

class UnifiedL2;

// L2Port — one L1's view of the shared L2 (see UnifiedL2 below).
//
// Implements IMemory so an L1 `Cache` can take it as its backing with
// no idea it is shared. The two halves of IMemory go to different
// places, which is the whole point:
//
//   data  (load_*/store_*) → `data`, the side's own functional
//         authority (imem_ for the I side, dram_ for the D side).
//         The L2 is a timing overlay and holds no data, so the
//         Harvard split above it survives untouched — enabling the
//         L2 provably cannot change a committed value.
//   timing (ready/tick)    → the shared L2, through the hub's
//         arbitration.
class L2Port : public IMemory {
public:
    L2Port(UnifiedL2& hub, unsigned id, IMemory& data);

    uint32_t load_word(uint32_t addr) const override;
    uint16_t load_half(uint32_t addr) const override;
    uint8_t  load_byte(uint32_t addr) const override;
    void store_word(uint32_t addr, uint32_t val) override;
    void store_half(uint32_t addr, uint16_t val) override;
    void store_byte(uint32_t addr, uint8_t  val) override;

    bool ready(uint32_t addr, bool is_write = false) override;
    void tick() override;

private:
    UnifiedL2& hub_;
    unsigned   id_;
    IMemory&   data_;
};

// UnifiedL2 — one L2 `Cache` shared by both L1s, plus the arbitration
// that a shared level needs and a private one does not.
//
//     L1 I-cache ─┐                   ┌─ tags shared by both sides
//                 ├─ L2Port ─→ L2 ────┴─→ DRAM
//     L1 D-cache ─┘
//
// "Unified" is exactly this: ONE tag array, so a line the data side
// pulls in is already there for the other side and vice versa,
// and the two compete for the same capacity. That is the property
// worth modeling; it is also what forces arbitration, because `Cache`
// serves ONE blocking request at a time (mem_interface.h).
//
// Arbitration, in three rules:
//
//  1. **Ownership.** The first port to ask acquires the L2 and holds
//     it until its access completes; a port that asks meanwhile is
//     told "not ready" and retries. Serializing is correct, not a
//     simplification — a single-ported cache really can only serve
//     one access at a time.
//  2. **One clock, one advance.** The L2 ticks at most once per
//     simulated cycle no matter how many ports drive it, and ANY port
//     may be the one to advance it — not just the owner.
//  3. **A refused port gets first refusal on the next grant.** The
//     port that just gave the L2 up cannot immediately re-take it
//     while someone it turned away is still waiting.
//
// Rule 2's "any port" is load-bearing. The front end stops ticking
// entirely during a MEM stall (see architecture.md § Stalls), so if
// only the owner could advance the L2, an I-side owner frozen by a
// D-side stall would wedge the D-side that froze it — a deadlock, and
// the exact case an in-order core hits constantly. Letting the stalled
// D side drive the L2 through the I side's fill breaks it: the fill
// finishes, the L2 goes idle, and ownership passes.
//
// Rule 3 is what makes "passes" true. Core::tick() evaluates fetch
// before MEM, so the I side asks first every cycle; with no I-cache
// above it, it asks EVERY cycle. Plain first-come ownership then lets
// it re-take the L2 the instant it frees and the D side never runs
// again — a livelock (measured: 7 instructions in 5000 cycles). The
// reservation lapses if its holder stops asking, so a port that walks
// away from a request (a dropped prefetch) cannot wedge the other
// side in turn.
//
// Modeled cost of that hand-off: a port can lose ownership between
// asking and being served (its line is installed, but it was frozen
// when the L2 announced it). It then re-asks and pays a fresh L2 HIT
// rather than resuming — bounded, self-converging, and pessimistic in
// the honest direction. It never affects data.
//
// Inclusion: **NINE** (non-inclusive non-exclusive). Each level
// installs on its own miss and never back-invalidates the other. With
// a tags-only model this has no functional effect at all — only which
// tags are resident, hence future timing. True inclusion would need
// the L2 to reach up into the L1s on eviction; not worth it yet.
class UnifiedL2 {
public:
    // `backing` is the DRAM below the L2 — one memory, since a unified
    // L2 has one thing under it. `cycle` is the simulation's clock
    // counter, read (never written) to enforce rule 2; bind it to
    // SimStats::cycles, or to a plain counter in a test.
    // line_bytes, sets, and ways must be powers of two.
    UnifiedL2(IMemory& backing,
              const uint64_t& cycle,
              unsigned line_bytes,
              unsigned sets,
              unsigned hit_latency,
              unsigned ways = 1);

    // Create a port for one L1. `data` is that side's functional
    // authority (see L2Port). Ports are owned by the hub and outlive
    // every caller of attach().
    L2Port& attach(IMemory& data);

    // Inspection (tests, and the bench_run summary).
    uint64_t hits()   const { return l2_.hits(); }
    uint64_t misses() const { return l2_.misses(); }

private:
    friend class L2Port;

    bool port_ready(unsigned id, uint32_t addr, bool is_write);
    void port_tick();

    // Rule 3: is the next grant spoken for by someone other than `id`?
    // Only the port that last held the L2 is held off, and only while
    // the waiter is still actively asking.
    bool reserved_against(unsigned id) const;

    static constexpr unsigned kFree = ~0u;   // no port owns the L2

    Cache           l2_;
    const uint64_t& cycle_;
    unsigned        owner_      = kFree;
    unsigned        last_owner_ = kFree;  // who to hold off under rule 3
    unsigned        waiter_     = kFree;  // port refused most recently
    uint64_t        waiter_cyc_ = 0;      // when — the claim expires
    uint64_t        last_tick_  = ~0ULL;  // no cycle advanced yet

    std::vector<std::unique_ptr<L2Port>> ports_;
};

}  // namespace cpusim
