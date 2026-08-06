#include <gtest/gtest.h>
#include "cpusim/memory/cache.h"
#include "cpusim/memory/flat_mem.h"

using namespace cpusim;

static constexpr uint32_t BASE = 0x80000000u;
static constexpr size_t   SZ   = 0x10000u;

// Backing DRAM latency, and cache geometry used across these tests.
static constexpr unsigned DRAM_LAT = 10;
static constexpr unsigned LINE     = 16;
static constexpr unsigned SETS     = 4;
static constexpr unsigned HIT_LAT  = 2;

// Drive one blocking access to completion; return cycles it occupied.
static int access_cycles(IMemory& m, uint32_t addr) {
    int c = 1;
    while (!m.ready(addr)) { m.tick(); ++c; }
    m.tick();   // release the port for the next request
    return c;
}

// Drive one blocking STORE to completion; return cycles it occupied.
static int store_cycles(IMemory& m, uint32_t addr) {
    int c = 1;
    while (!m.ready(addr, /*is_write=*/true)) { m.tick(); ++c; }
    m.tick();   // release the port for the next request
    return c;
}

// Two addresses 64 B apart (SETS*LINE) collide in the same set.
static constexpr uint32_t A = BASE;          // index 0
static constexpr uint32_t B = BASE + 0x40;   // index 0, different tag

// ── Timing ────────────────────────────────────────────────────────────

TEST(Cache, ColdMissThenHit) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT);

    // First touch misses: pays the DRAM fetch plus the hit latency.
    EXPECT_EQ(access_cycles(c, A), DRAM_LAT + HIT_LAT);
    EXPECT_FALSE(c.last_was_hit());

    // Second touch of the same line hits: just the hit latency.
    EXPECT_EQ(access_cycles(c, A), HIT_LAT);
    EXPECT_TRUE(c.last_was_hit());
}

TEST(Cache, SpatialLocalityWithinLine) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT);

    EXPECT_EQ(access_cycles(c, A),     DRAM_LAT + HIT_LAT);  // miss
    EXPECT_EQ(access_cycles(c, A + 4), HIT_LAT);             // same line → hit
    EXPECT_TRUE(c.last_was_hit());
}

TEST(Cache, DirectMappedConflictEvicts) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT);

    EXPECT_EQ(access_cycles(c, A), DRAM_LAT + HIT_LAT);  // miss, install A
    EXPECT_EQ(access_cycles(c, A), HIT_LAT);             // hit
    EXPECT_EQ(access_cycles(c, B), DRAM_LAT + HIT_LAT);  // conflict miss, evict A
    EXPECT_EQ(access_cycles(c, A), DRAM_LAT + HIT_LAT);  // A was evicted → miss
}

TEST(Cache, DistinctSetsDoNotConflict) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT);

    EXPECT_EQ(access_cycles(c, A),        DRAM_LAT + HIT_LAT);  // index 0 miss
    EXPECT_EQ(access_cycles(c, A + LINE), DRAM_LAT + HIT_LAT);  // index 1 miss
    EXPECT_EQ(access_cycles(c, A),        HIT_LAT);             // still cached
    EXPECT_EQ(access_cycles(c, A + LINE), HIT_LAT);             // still cached
}

// ── Set-associativity + PLRU ──────────────────────────────────────────

// Two-way: A and B collide in one set but coexist in two ways, so the
// direct-mapped conflict (see DirectMappedConflictEvicts) disappears.
TEST(Cache, TwoWayKeepsBothConflictingLines) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT, /*ways=*/2);

    EXPECT_EQ(access_cycles(c, A), DRAM_LAT + HIT_LAT);  // miss, way 0
    EXPECT_EQ(access_cycles(c, B), DRAM_LAT + HIT_LAT);  // miss, way 1
    EXPECT_EQ(access_cycles(c, A), HIT_LAT);             // both resident
    EXPECT_EQ(access_cycles(c, B), HIT_LAT);
}

// A cold set fills its invalid ways first — no eviction until full.
TEST(Cache, FirstInvalidWayFillsBeforeEviction) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT, /*ways=*/4);

    // Four distinct tags in the same set (stride = SETS*LINE = 0x40).
    const uint32_t stride = SETS * LINE;
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(access_cycles(c, A + i * stride), DRAM_LAT + HIT_LAT);
    // All four still resident — set was filled, nothing evicted.
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(access_cycles(c, A + i * stride), HIT_LAT);
}

// Tree-PLRU evicts the least-recently-used way. Fill w0..w3, then touch
// them so w0 is LRU; the fifth distinct tag must evict exactly w0's tag.
TEST(Cache, PlruEvictsLeastRecentlyUsed) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT, /*ways=*/4);

    const uint32_t stride = SETS * LINE;
    const uint32_t w0 = A, w1 = A + stride, w2 = A + 2*stride,
                   w3 = A + 3*stride, w4 = A + 4*stride;

    for (uint32_t a : {w0, w1, w2, w3})      // fill order w0,w1,w2,w3
        EXPECT_EQ(access_cycles(c, a), DRAM_LAT + HIT_LAT);

    // Re-touch w1,w2,w3 so w0 becomes the pseudo-LRU (oldest) way.
    for (uint32_t a : {w1, w2, w3})
        EXPECT_EQ(access_cycles(c, a), HIT_LAT);

    // A fifth tag evicts the LRU way (w0's slot) and leaves w1..w3 intact.
    EXPECT_EQ(access_cycles(c, w4), DRAM_LAT + HIT_LAT);  // evicts w0
    EXPECT_EQ(access_cycles(c, w1), HIT_LAT);             // still resident
    EXPECT_EQ(access_cycles(c, w2), HIT_LAT);
    EXPECT_EQ(access_cycles(c, w3), HIT_LAT);
    EXPECT_EQ(access_cycles(c, w0), DRAM_LAT + HIT_LAT);  // w0 was evicted
}

// ── Write-back ────────────────────────────────────────────────────────

// A store that hits is served at hit latency (no backing traffic); the
// line simply becomes dirty. A following access still hits.
TEST(Cache, StoreHitServesAtHitLatency) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT, /*ways=*/1);

    EXPECT_EQ(access_cycles(c, A), DRAM_LAT + HIT_LAT);  // load miss (clean)
    EXPECT_EQ(store_cycles(c, A),  HIT_LAT);             // store hit
    EXPECT_EQ(access_cycles(c, A), HIT_LAT);             // still resident
}

// Evicting a CLEAN victim costs a plain miss; evicting a DIRTY victim
// costs an extra writeback (flush + fetch + hit).
TEST(Cache, DirtyEvictionPaysWriteback) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT, /*ways=*/1);

    EXPECT_EQ(access_cycles(c, A), DRAM_LAT + HIT_LAT);  // A clean
    EXPECT_EQ(access_cycles(c, B), DRAM_LAT + HIT_LAT);  // evict clean A
    EXPECT_EQ(access_cycles(c, A), DRAM_LAT + HIT_LAT);  // evict clean B
    EXPECT_EQ(store_cycles(c, A),  HIT_LAT);             // dirty A
    EXPECT_EQ(access_cycles(c, B), 2 * DRAM_LAT + HIT_LAT);  // flush dirty A
}

// A store miss is write-allocate and installs the line DIRTY, so the
// next eviction of that line pays the writeback.
TEST(Cache, StoreMissInstallsDirty) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT, /*ways=*/1);

    EXPECT_EQ(store_cycles(c, A),  DRAM_LAT + HIT_LAT);      // write-allocate
    EXPECT_EQ(access_cycles(c, B), 2 * DRAM_LAT + HIT_LAT);  // A was dirty
}

// ── Data path (delegation) ────────────────────────────────────────────

TEST(Cache, DataDelegatesToBacking) {
    FlatMem dram(BASE, SZ, DRAM_LAT);
    Cache   c(dram, LINE, SETS, HIT_LAT);

    c.store_word(A, 0xDEADBEEFu);
    EXPECT_EQ(c.load_word(A), 0xDEADBEEFu);   // via backing
    EXPECT_EQ(dram.load_word(A), 0xDEADBEEFu); // data really lives in DRAM

    c.store_byte(A + 1, 0xAB);
    EXPECT_EQ(c.load_byte(A + 1), 0xAB);
}

// ── I-cache (Cache pointed at an instruction backing) ───────────────────
//
// The I-cache is just a Cache instance over imem_ instead of dram_ — same
// class, same timing rules. These mirror the D-cache timing tests above
// to prove that reuse holds; the fetch stage does not drive this port
// yet (Step 4), so there is no pipeline-level test here.

TEST(Cache, InstructionColdMissThenHit) {
    FlatMem imem(BASE, SZ, DRAM_LAT);   // stands in for instruction memory
    Cache   ic(imem, LINE, SETS, HIT_LAT);

    EXPECT_EQ(access_cycles(ic, A), DRAM_LAT + HIT_LAT);  // cold miss
    EXPECT_FALSE(ic.last_was_hit());
    EXPECT_EQ(access_cycles(ic, A), HIT_LAT);             // hit
    EXPECT_TRUE(ic.last_was_hit());
}

TEST(Cache, InstructionSpatialLocalityWithinLine) {
    FlatMem imem(BASE, SZ, DRAM_LAT);
    Cache   ic(imem, LINE, SETS, HIT_LAT);

    EXPECT_EQ(access_cycles(ic, A),     DRAM_LAT + HIT_LAT);  // miss
    EXPECT_EQ(access_cycles(ic, A + 4), HIT_LAT);             // same line
}

TEST(Cache, InstructionAssociativityAvoidsConflict) {
    FlatMem imem(BASE, SZ, DRAM_LAT);
    Cache   ic(imem, LINE, SETS, HIT_LAT, /*ways=*/2);

    EXPECT_EQ(access_cycles(ic, A), DRAM_LAT + HIT_LAT);  // miss, way 0
    EXPECT_EQ(access_cycles(ic, B), DRAM_LAT + HIT_LAT);  // miss, way 1
    EXPECT_EQ(access_cycles(ic, A), HIT_LAT);             // both resident
    EXPECT_EQ(access_cycles(ic, B), HIT_LAT);
}

// ── Prefetch (background next-line engine) ──────────────────────────
//
// Off by default (all tests above run unmodified with prefetch
// disabled); these exercise the engine explicitly enabled.

// Demand access to L0 queues a background prefetch of L1 immediately;
// it advances on every cycle L0's own hit-latency leaves the backing
// port idle. By the time the demand stream reaches L1, some of its
// fetch time is already paid for — a PARTIAL hit, cheaper than a cold
// miss (test 11 in the redesign plan).
TEST(Cache, PrefetchTurnsNextLineIntoPartialHit) {
    FlatMem imem(BASE, SZ, DRAM_LAT);
    Cache   ic(imem, LINE, SETS, HIT_LAT, /*ways=*/1,
              /*prefetch_enabled=*/true, /*degree=*/1);

    const uint32_t L0 = A, L1 = A + LINE;

    EXPECT_EQ(access_cycles(ic, L0), DRAM_LAT + HIT_LAT);  // cold miss
    EXPECT_LT(access_cycles(ic, L1), DRAM_LAT + HIT_LAT);  // partial
    EXPECT_FALSE(ic.last_was_hit());  // was in flight, not resident
}

// With prefetch off, the same walk pays a full miss on every line —
// the counterfactual that makes the test above meaningful.
TEST(Cache, WithoutPrefetchNextLineIsAFullMiss) {
    FlatMem imem(BASE, SZ, DRAM_LAT);
    Cache   ic(imem, LINE, SETS, HIT_LAT, /*ways=*/1);

    const uint32_t L0 = A, L1 = A + LINE;

    EXPECT_EQ(access_cycles(ic, L0), DRAM_LAT + HIT_LAT);
    EXPECT_EQ(access_cycles(ic, L1), DRAM_LAT + HIT_LAT);  // full miss
}

// Prefetching never changes committed data (Option A: data always
// lives in the backing). Walk several sequential lines — enough to
// exercise chaining (degree=2) and eviction of an unused prefetch —
// and confirm every value the cache returns still matches the backing
// exactly (test 12 in the redesign plan).
TEST(Cache, PrefetchNeverChangesCommittedData) {
    FlatMem imem(BASE, SZ, DRAM_LAT);
    Cache   ic(imem, LINE, SETS, HIT_LAT, /*ways=*/1,
              /*prefetch_enabled=*/true, /*degree=*/2);

    const uint32_t stride = LINE;
    for (int i = 0; i < 4; ++i)
        imem.write_bytes(A + i * stride,
                         reinterpret_cast<const uint8_t*>(&i), sizeof(i));

    for (int i = 0; i < 4; ++i) access_cycles(ic, A + i * stride);

    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(ic.load_word(A + i * stride),
                 imem.load_word(A + i * stride));
}
