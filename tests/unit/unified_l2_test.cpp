#include <gtest/gtest.h>
#include "cpusim/memory/flat_mem.h"
#include "cpusim/memory/unified_l2.h"

using namespace cpusim;

static constexpr uint32_t BASE = 0x80000000u;
static constexpr size_t   SZ   = 0x10000u;

static constexpr unsigned DRAM_LAT = 10;

// L1s are deliberately tiny (2 sets) and the L2 roomy (8 sets), so a
// pair of addresses can conflict in an L1 and still coexist in the L2 —
// the capacity tier that justifies having an L2 at all.
static constexpr unsigned LINE    = 16;
static constexpr unsigned L1_SETS = 2;
static constexpr unsigned L1_HIT  = 1;
static constexpr unsigned L2_SETS = 8;
static constexpr unsigned L2_HIT  = 4;

// The §3 latency table, derived rather than typed twice: a miss costs
// whatever the level below took to serve, plus this level's own hit.
static constexpr int HIT       = L1_HIT;                        //  1
static constexpr int L2_HIT_C  = L2_HIT + L1_HIT;               //  5
static constexpr int L2_MISS_C = DRAM_LAT + L2_HIT + L1_HIT;    // 15

// Addresses. L1 sets repeat every L1_SETS*LINE = 32 B, L2 sets every
// L2_SETS*LINE = 128 B.
static constexpr uint32_t A = BASE;          // L1 set 0, L2 set 0
static constexpr uint32_t C = BASE + 32;   // L1 set 0 too, L2 set 2

// A two-level rig: DRAM, one shared L2, and an L1 per side over its own
// port. `cycle` is the shared clock the hub arbitrates on — in the
// simulator this is SimStats::cycles, driven by Core::tick().
struct Rig {
    uint64_t  cycle = 0;
    FlatMem   imem{BASE, SZ, DRAM_LAT};
    FlatMem   dram{BASE, SZ, DRAM_LAT};
    UnifiedL2 hub{dram, cycle, LINE, L2_SETS, L2_HIT};
    L2Port&   iport = hub.attach(imem);
    L2Port&   dport = hub.attach(dram);
    Cache     l1i{iport, LINE, L1_SETS, L1_HIT};
    Cache     l1d{dport, LINE, L1_SETS, L1_HIT};
};

// Drive one access through an L1 to completion, advancing the shared
// clock every tick. Returns the cycles the access occupied.
static int access_cycles(Rig& r, Cache& l1, uint32_t addr,
                         bool is_write = false) {
    int n = 1;
    while (!l1.ready(addr, is_write)) { l1.tick(); ++r.cycle; ++n; }
    l1.tick(); ++r.cycle;   // release the port for the next request
    return n;
}

// ── The latency table ─────────────────────────────────────────────────

// Each tier of the hierarchy costs what it should, and the tiers
// compose: a miss pays the level below plus its own hit latency. This
// is the model's correctness proof — the Spike diff validates data and
// is blind to timing by construction (memory-model.md § no oracle).
TEST(UnifiedL2, ThreeTierAccessCosts) {
    Rig r;

    // Cold: nothing is resident anywhere, so this walks to DRAM.
    EXPECT_EQ(access_cycles(r, r.l1d, A), L2_MISS_C);

    // Now resident in the L1 — the L2 is not consulted at all.
    EXPECT_EQ(access_cycles(r, r.l1d, A), HIT);

    // Evict A from the L1 (C collides there), then come back for it.
    // The L2 still holds A, so this is the middle tier: an L1 miss the
    // L2 absorbs without touching DRAM.
    EXPECT_EQ(access_cycles(r, r.l1d, C), L2_MISS_C);
    EXPECT_EQ(access_cycles(r, r.l1d, A), L2_HIT_C);
}

// Spatial locality still works a level down: a second line in the same
// L1 set thrashes the L1 but the L2 keeps both, so a ping-pong that
// would pay full DRAM cost without an L2 pays the L2 hit cost instead.
TEST(UnifiedL2, L2AbsorbsL1ConflictThrashing) {
    Rig r;
    EXPECT_EQ(access_cycles(r, r.l1d, A), L2_MISS_C);   // cold
    EXPECT_EQ(access_cycles(r, r.l1d, C), L2_MISS_C);   // cold, evicts A

    // Both lines are now in the L2 (different L2 sets) but only one
    // fits in the L1 set. Every access is an L1 miss; none reach DRAM.
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(access_cycles(r, r.l1d, A), L2_HIT_C);
        EXPECT_EQ(access_cycles(r, r.l1d, C), L2_HIT_C);
    }
    EXPECT_EQ(r.hub.misses(), 2u);   // only the two compulsory misses
}

// ── "Unified": one tag array, shared by both sides ────────────────────

// The defining property. A line the data side pulled into the L2 is
// already warm for the instruction side — with two private L2s this
// would be a second full DRAM trip.
TEST(UnifiedL2, LineFetchedByOneSideIsWarmForTheOther) {
    Rig r;
    EXPECT_EQ(access_cycles(r, r.l1d, A), L2_MISS_C);   // D side pays
    EXPECT_EQ(access_cycles(r, r.l1i, A), L2_HIT_C);   // I rides along
    EXPECT_EQ(r.hub.misses(), 1u);
    EXPECT_EQ(r.hub.hits(),   1u);
}

// The flip side of sharing: the two sides also compete for capacity.
// An instruction line can evict a data line, which private L2s could
// never do to each other.
TEST(UnifiedL2, SidesCompeteForTheSameCapacity) {
    Rig r;
    // 128 B apart → same L2 set, different tags → they collide.
    const uint32_t evictor = A + 128;

    EXPECT_EQ(access_cycles(r, r.l1d, A), L2_MISS_C);
    EXPECT_EQ(access_cycles(r, r.l1i, evictor), L2_MISS_C);  // evicts A

    // A is still in the D-side L1, so ask through the other L1 to see
    // the L2 state: it is gone, and this is a full DRAM trip again.
    EXPECT_EQ(access_cycles(r, r.l1i, A), L2_MISS_C);
}

// ── Arbitration ───────────────────────────────────────────────────────

// One cycle of the simulator: both stages ask, then both latch. MEM
// latches before fetch in Core::tick(), so the D side drives the shared
// L2 first — mirrored here so the test exercises the real ordering.
static void step(Rig& r, uint32_t daddr, uint32_t iaddr,
                 bool& dserved, bool& iserved) {
    dserved = r.l1d.ready(daddr, false);
    iserved = r.l1i.ready(iaddr, false);
    r.l1d.tick();
    r.l1i.tick();
    ++r.cycle;
}

// A single-ported L2 can only serve one access at a time, so two
// same-cycle misses are strictly serial. The dedupe is what makes this
// true: without it both L1s would tick the L2 each cycle and their
// fills would complete at half latency.
TEST(UnifiedL2, ConcurrentMissesSerialize) {
    Rig r;
    const uint32_t iaddr = A + 128;   // a different line from A

    int dcycle = 0, icycle = 0;
    bool dserved = false, iserved = false;
    for (int n = 1; n <= 200 && !(dcycle && icycle); ++n) {
        step(r, A, iaddr, dserved, iserved);
        if (dserved && !dcycle) dcycle = n;
        if (iserved && !icycle) icycle = n;
    }

    ASSERT_NE(dcycle, 0);
    ASSERT_NE(icycle, 0);

    // D drives first, wins the port, and pays a full miss.
    EXPECT_EQ(dcycle, L2_MISS_C);

    // I's fill then costs another full miss, and the two do not
    // overlap in the L2 at all. They overlap by exactly L1_HIT: the
    // last leg of D's access is its own L1 serving the fetched line,
    // which does not touch the L2 — so ownership passes that much
    // before D reports served. Any larger overlap would mean the L2
    // advanced twice in a cycle.
    EXPECT_EQ(icycle - dcycle, L2_MISS_C - L1_HIT);
}

// Deadlock guard. The front end stops ticking entirely during a MEM
// stall (architecture.md § Stalls), so an I-side owner can freeze mid
// fill while the D side spins waiting for the L2 it holds. Any port
// may advance the shared L2 precisely so the stalled side can drive
// the frozen owner's fill to completion and take the port after it.
TEST(UnifiedL2, StalledSideDrivesAFrozenOwnersFill) {
    Rig r;

    // The I side claims the L2, then freezes: it never ticks again.
    EXPECT_FALSE(r.l1i.ready(A, false));
    r.l1i.tick();
    ++r.cycle;

    // The D side, alone, must still make progress.
    int n = 1;
    for (; n <= 200 && !r.l1d.ready(C, false); ++n) {
        r.l1d.tick();
        ++r.cycle;
    }
    EXPECT_LE(n, 200) << "D side wedged behind a frozen L2 owner";
}

// Fairness. Core::tick() evaluates fetch before MEM, so the I side asks
// first every cycle — and with no I-cache above it to absorb fetches,
// it asks EVERY cycle, never idling. Under plain first-come ownership
// it re-takes the L2 the instant it frees and the D side never runs
// again. That is not hypothetical: before the reservation rule existed
// this configuration livelocked a real benchmark at 7 instructions in
// 5000 cycles.
TEST(UnifiedL2, RelentlessRequesterCannotStarveTheOtherSide) {
    Rig r;
    bool dserved = false;

    for (int n = 1; n <= 500 && !dserved; ++n) {
        // The I side asks first, for a fresh line every time, and never
        // stops asking. The D side wants one line and keeps retrying.
        r.iport.ready(BASE + 0x1000 + LINE * (n % 8), false);
        dserved = r.dport.ready(A, false);
        r.dport.tick();     // MEM latches before fetch, as in Core
        r.iport.tick();
        ++r.cycle;
    }

    EXPECT_TRUE(dserved) << "D side starved by a relentless I side";
}

// ── Data path ─────────────────────────────────────────────────────────

// The L2 is a timing overlay and carries no data: each port reads and
// writes its own side's authority. This is what keeps the Harvard split
// intact with the L2 on, and why enabling it cannot change a committed
// value — the guarantee the whole timing study rests on.
TEST(UnifiedL2, PortsKeepTheirOwnDataAuthority) {
    Rig r;
    r.imem.store_word(A, 0x11111111u);
    r.dram.store_word(A, 0x22222222u);

    EXPECT_EQ(r.iport.load_word(A), 0x11111111u);
    EXPECT_EQ(r.dport.load_word(A), 0x22222222u);

    // Sharing the L2 for timing does not leak a store across sides.
    access_cycles(r, r.l1d, A, /*is_write=*/true);
    r.l1d.store_word(A, 0x33333333u);
    EXPECT_EQ(r.dram.load_word(A), 0x33333333u);
    EXPECT_EQ(r.imem.load_word(A), 0x11111111u);
}

// A store that dirties an L1 line is flushed to the L2 on eviction, and
// the L2 takes the write as a write (dirtying its own line) rather than
// silently dropping it — the is_write flag has to survive the port.
TEST(UnifiedL2, DirtyL1EvictionReachesTheL2AsAWrite) {
    Rig r;
    access_cycles(r, r.l1d, A, /*is_write=*/true);   // installs dirty
    const uint64_t before = r.hub.hits() + r.hub.misses();

    // C evicts dirty A: a writeback access for A, then the fill for C.
    access_cycles(r, r.l1d, C);
    EXPECT_EQ(r.hub.hits() + r.hub.misses(), before + 2);
}
