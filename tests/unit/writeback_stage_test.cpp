#include <gtest/gtest.h>
#include "cpusim/regfile.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/pipeline/writeback.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/isa/rv32i/decoder.h"

using namespace cpusim;
using namespace cpusim::pipeline;
using namespace cpusim::rv32i;

static MemWb make_memwb(Op op, uint8_t rd, uint32_t wb_val) {
    MemWb w;
    w.op = op; w.rd = rd; w.wb_val = wb_val; w.valid = true;
    return w;
}

struct Fixture {
    RegFile          rf;
    Latch<MemWb>     in;
    WritebackStage   wb{rf, in};

    void load(const MemWb& v) { in.write(v); in.latch(); }
};

// ── Basic writeback ───────────────────────────────────────────────────────────

TEST(WbStage, WritesResultToRegFile) {
    Fixture f;
    f.load(make_memwb(Op::ADDI, 3, 42));
    f.wb.evaluate(); f.wb.latch();
    EXPECT_EQ(f.rf.read(3), 42u);
}

TEST(WbStage, LoadResultWrittenBack) {
    Fixture f;
    f.load(make_memwb(Op::LW, 5, 0xDEADBEEFu));
    f.wb.evaluate(); f.wb.latch();
    EXPECT_EQ(f.rf.read(5), 0xDEADBEEFu);
}

TEST(WbStage, JalLinkRegisterWritten) {
    Fixture f;
    f.load(make_memwb(Op::JAL, 1, 0x80000004u));
    f.wb.evaluate(); f.wb.latch();
    EXPECT_EQ(f.rf.read(1), 0x80000004u);
}

// ── Ops that must NOT write ───────────────────────────────────────────────────

TEST(WbStage, StoreDoesNotWriteRegFile) {
    Fixture f;
    f.rf.write(2, 0xABCDu);
    f.load(make_memwb(Op::SW, 2, 0));  // rd=2 but SW must not write
    f.wb.evaluate(); f.wb.latch();
    EXPECT_EQ(f.rf.read(2), 0xABCDu);  // unchanged
}

TEST(WbStage, BranchDoesNotWriteRegFile) {
    Fixture f;
    f.rf.write(1, 99u);
    f.load(make_memwb(Op::BEQ, 1, 0));
    f.wb.evaluate(); f.wb.latch();
    EXPECT_EQ(f.rf.read(1), 99u);  // unchanged
}

// ── Bubble ────────────────────────────────────────────────────────────────────

TEST(WbStage, BubbleDoesNotWriteRegFile) {
    Fixture f;
    f.rf.write(1, 55u);
    // in left at default (valid=false)
    f.wb.evaluate(); f.wb.latch();
    EXPECT_EQ(f.rf.read(1), 55u);  // unchanged
}

// ── pending_write accessors ───────────────────────────────────────────────────

TEST(WbStage, PendingWriteReflectsAfterEvaluate) {
    Fixture f;
    f.load(make_memwb(Op::ADD, 4, 7u));
    f.wb.evaluate();
    EXPECT_TRUE(f.wb.pending_write());
    EXPECT_EQ(f.wb.pending_rd(),  4u);
    EXPECT_EQ(f.wb.pending_val(), 7u);
}

TEST(WbStage, PendingWriteClearedAfterLatch) {
    Fixture f;
    f.load(make_memwb(Op::ADD, 4, 7u));
    f.wb.evaluate(); f.wb.latch();
    EXPECT_FALSE(f.wb.pending_write());
}

TEST(WbStage, NoPendingWriteForStore) {
    Fixture f;
    f.load(make_memwb(Op::SW, 0, 0));
    f.wb.evaluate();
    EXPECT_FALSE(f.wb.pending_write());
}
