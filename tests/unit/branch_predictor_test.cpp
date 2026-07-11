#include <gtest/gtest.h>
#include "cpusim/uarch/branch_predictor.h"

using namespace cpusim;

static constexpr uint32_t PC = 0x80000100u;
static constexpr uint32_t TGT = 0x80000080u;   // a backward branch target

// ── BTB gating ────────────────────────────────────────────────────────

TEST(BranchPredictor, ColdIsNotTaken) {
    BranchPredictor bp(256, 64);
    // Never seen this PC → BTB miss → predict not-taken.
    EXPECT_FALSE(bp.predict(PC).taken);
}

TEST(BranchPredictor, PredictsAfterInstall) {
    BranchPredictor bp(256, 64);
    // One taken conditional resolution installs the BTB and nudges the
    // counter from weakly-not-taken(1) to weakly-taken(2).
    bp.update(PC, /*is_cond=*/true, /*taken=*/true, TGT);

    auto p = bp.predict(PC);
    EXPECT_TRUE(p.taken);
    EXPECT_EQ(p.target, TGT);
}

// ── 2-bit hysteresis ──────────────────────────────────────────────────

TEST(BranchPredictor, TwoBitHysteresis) {
    BranchPredictor bp(256, 64);
    // Drive strongly-taken (counter saturates at 3).
    for (int i = 0; i < 3; i++) bp.update(PC, true, true, TGT);
    EXPECT_TRUE(bp.predict(PC).taken);

    // A single not-taken drops 3->2: still predicts taken (hysteresis).
    bp.update(PC, true, false, TGT);
    EXPECT_TRUE(bp.predict(PC).taken);

    // A second not-taken drops 2->1: now flips to not-taken.
    bp.update(PC, true, false, TGT);
    EXPECT_FALSE(bp.predict(PC).taken);
}

// ── Unconditional (JAL) is always taken once known ────────────────────

TEST(BranchPredictor, UnconditionalAlwaysTaken) {
    BranchPredictor bp(256, 64);
    bp.update(PC, /*is_cond=*/false, /*taken=*/true, TGT);

    auto p = bp.predict(PC);
    EXPECT_TRUE(p.taken);
    EXPECT_EQ(p.target, TGT);
}

// ── BTB tags prevent aliasing ─────────────────────────────────────────

TEST(BranchPredictor, TagAvoidsFalseHit) {
    BranchPredictor bp(256, /*btb_entries=*/8);
    // Two PCs 8*4 = 32 bytes apart share a BTB index but differ in tag.
    const uint32_t a = 0x80001000u;
    const uint32_t b = a + 8 * 4;
    bp.update(a, true, true, TGT);

    EXPECT_TRUE(bp.predict(a).taken);    // installed entry
    EXPECT_FALSE(bp.predict(b).taken);   // same index, wrong tag → miss
}
