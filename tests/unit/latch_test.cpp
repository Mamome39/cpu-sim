#include <gtest/gtest.h>
#include "cpusim/uarch/latch.h"

using namespace cpusim;

// ── Basic two-phase protocol ──────────────────────────────────────────────────

TEST(Latch, InitialValueIsDefault) {
    Latch<int> l;
    EXPECT_EQ(l.read(), 0);
}

TEST(Latch, WriteDoesNotChangeReadBeforeLatch) {
    Latch<int> l;
    l.write(42);
    EXPECT_EQ(l.read(), 0);  // shadow written, register unchanged
}

TEST(Latch, LatchCommitsShadowToRegister) {
    Latch<int> l;
    l.write(42);
    l.latch();
    EXPECT_EQ(l.read(), 42);
}

TEST(Latch, MultiCycleAdvance) {
    Latch<int> l;
    l.write(1); l.latch();
    l.write(2); l.latch();
    l.write(3); l.latch();
    EXPECT_EQ(l.read(), 3);
}

// ── Stall: skip latch() ───────────────────────────────────────────────────────

TEST(Latch, StallHoldsValue) {
    Latch<int> l;
    l.write(10); l.latch();  // cycle 1: commit 10

    l.write(20);             // cycle 2: upstream writes 20
    // stall — do NOT call latch()
    EXPECT_EQ(l.read(), 10); // register still holds 10
}

TEST(Latch, StallThenResume) {
    Latch<int> l;
    l.write(10); l.latch();

    l.write(20);             // stalled — shadow=20, reg=10
    l.write(20); l.latch();  // resume — commits 20
    EXPECT_EQ(l.read(), 20);
}

// ── Flush ─────────────────────────────────────────────────────────────────────

TEST(Latch, FlushClearsRegisterToDefault) {
    Latch<int> l;
    l.write(99); l.latch();
    l.flush();
    EXPECT_EQ(l.read(), 0);
}

TEST(Latch, FlushAlsoClearsShadow) {
    Latch<int> l;
    l.write(99); l.latch();
    l.flush();
    l.latch();               // commit shadow (which was also cleared)
    EXPECT_EQ(l.read(), 0);
}

// ── Reset ─────────────────────────────────────────────────────────────────────

TEST(Latch, ResetClearsAll) {
    Latch<int> l;
    l.write(55); l.latch();
    l.write(77);
    l.reset();
    EXPECT_EQ(l.read(), 0);
    l.latch();
    EXPECT_EQ(l.read(), 0);  // shadow was reset too
}

// ── Struct payload ────────────────────────────────────────────────────────────

TEST(Latch, WorksWithStruct) {
    struct Payload { int x = 0; int y = 0; };

    Latch<Payload> l;
    l.write({3, 7});
    EXPECT_EQ(l.read().x, 0); // not yet latched
    l.latch();
    EXPECT_EQ(l.read().x, 3);
    EXPECT_EQ(l.read().y, 7);
}

TEST(Latch, FlushStructResetsToDefault) {
    struct Payload { int x = 0; };
    Latch<Payload> l;
    l.write({42}); l.latch();
    l.flush();
    EXPECT_EQ(l.read().x, 0);
}
