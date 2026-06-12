#include <gtest/gtest.h>
#include "cpusim/memory/flat_mem.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"

using namespace cpusim;
using namespace cpusim::pipeline;

// Helpers — load a word into memory at a given address.
static void poke(FlatMem& m, uint32_t addr, uint32_t word) {
    m.store_word(addr, word);
}

static constexpr uint32_t BASE = 0x80000000;
static constexpr size_t   SZ   = 0x1000;  // 4 KiB

// ── Basic fetch ───────────────────────────────────────────────────────────────

TEST(Fetch, FetchesInstructionAtResetPc) {
    FlatMem mem(BASE, SZ);
    poke(mem, BASE, 0xDEADBEEFu);

    Latch<IfId> out;
    FetchStage  fetch(mem, out, BASE);

    fetch.evaluate();
    fetch.latch();

    EXPECT_EQ(out.read().pc,  BASE);
    EXPECT_EQ(out.read().raw, 0xDEADBEEFu);
    EXPECT_TRUE(out.read().valid);
}

TEST(Fetch, PcAdvancesByFourEachCycle) {
    FlatMem mem(BASE, SZ);
    poke(mem, BASE,     0x00000013u); // nop
    poke(mem, BASE + 4, 0x00100093u); // addi x1, x0, 1

    Latch<IfId> out;
    FetchStage  fetch(mem, out, BASE);

    fetch.evaluate(); fetch.latch();
    EXPECT_EQ(out.read().pc, BASE);

    fetch.evaluate(); fetch.latch();
    EXPECT_EQ(out.read().pc,  BASE + 4);
    EXPECT_EQ(out.read().raw, 0x00100093u);
}

TEST(Fetch, PcAfterTwoCyclesIsBaseEight) {
    FlatMem    mem(BASE, SZ);
    Latch<IfId> out;
    FetchStage  fetch(mem, out, BASE);

    fetch.evaluate(); fetch.latch();
    fetch.evaluate(); fetch.latch();

    EXPECT_EQ(fetch.pc(), BASE + 8);
}

// ── Stall ─────────────────────────────────────────────────────────────────────

TEST(Fetch, StallHoldsPcAndLatch) {
    FlatMem mem(BASE, SZ);
    poke(mem, BASE, 0xAABBCCDDu);

    Latch<IfId> out;
    FetchStage  fetch(mem, out, BASE);

    fetch.evaluate(); fetch.latch();     // cycle 1: commit BASE
    EXPECT_EQ(fetch.pc(), BASE + 4);

    fetch.set_stall(true);
    fetch.evaluate();                    // evaluates but stall gates latch
    fetch.latch();

    // PC must not have moved
    EXPECT_EQ(fetch.pc(), BASE + 4);
    // Latch still holds the result from cycle 1
    EXPECT_EQ(out.read().pc, BASE);
}

TEST(Fetch, StallIsResetAfterOneCycle) {
    FlatMem    mem(BASE, SZ);
    Latch<IfId> out;
    FetchStage  fetch(mem, out, BASE);

    fetch.set_stall(true);
    fetch.evaluate(); fetch.latch();  // stalled

    // stall signal was cleared inside latch() — no need to re-set
    fetch.evaluate(); fetch.latch();  // normal cycle
    EXPECT_EQ(fetch.pc(), BASE + 4);
}

// ── Redirect (branch / jump) ──────────────────────────────────────────────────

TEST(Fetch, RedirectChangesPcAndFlushesLatch) {
    FlatMem mem(BASE, SZ);
    poke(mem, BASE,          0x00000013u); // nop at BASE
    poke(mem, BASE + 0x100,  0x00100093u); // target instruction

    Latch<IfId> out;
    FetchStage  fetch(mem, out, BASE);

    fetch.evaluate(); fetch.latch();     // fetch nop at BASE

    // EX stage signals a redirect to BASE+0x100
    fetch.set_redirect(true, BASE + 0x100);
    fetch.evaluate();
    fetch.latch();

    // IF/ID latch must be a bubble (flushed)
    EXPECT_FALSE(out.read().valid);
    // PC must be at the redirect target
    EXPECT_EQ(fetch.pc(), BASE + 0x100);
}

TEST(Fetch, AfterRedirectFetchesFromTarget) {
    FlatMem mem(BASE, SZ);
    poke(mem, BASE + 0x100, 0xCAFEBABEu);

    Latch<IfId> out;
    FetchStage  fetch(mem, out, BASE);

    fetch.set_redirect(true, BASE + 0x100);
    fetch.evaluate(); fetch.latch();   // redirect + flush

    fetch.evaluate(); fetch.latch();   // now fetch from target

    EXPECT_EQ(out.read().pc,  BASE + 0x100);
    EXPECT_EQ(out.read().raw, 0xCAFEBABEu);
    EXPECT_TRUE(out.read().valid);
}

TEST(Fetch, RedirectIsResetAfterOneCycle) {
    FlatMem    mem(BASE, SZ);
    Latch<IfId> out;
    FetchStage  fetch(mem, out, BASE);

    fetch.set_redirect(true, BASE + 0x100);
    fetch.evaluate(); fetch.latch();

    // redirect cleared — next cycle fetches from target and advances
    fetch.evaluate(); fetch.latch();
    EXPECT_EQ(fetch.pc(), BASE + 0x104);
}
