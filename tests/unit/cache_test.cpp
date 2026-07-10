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
