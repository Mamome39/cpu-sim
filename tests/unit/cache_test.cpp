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
