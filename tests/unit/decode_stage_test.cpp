#include <gtest/gtest.h>
#include "cpusim/regfile.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/pipeline/decode.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/isa/rv32i/decoder.h"

using namespace cpusim;
using namespace cpusim::pipeline;
using namespace cpusim::rv32i;

static constexpr uint32_t PC = 0x80000000;

// addi x1, x0, 42  →  imm=42, rd=1, rs1=0
static constexpr uint32_t ADDI_X1_X0_42 = 0x02A00093u;

// add x3, x1, x2  →  rd=3, rs1=1, rs2=2
static constexpr uint32_t ADD_X3_X1_X2 = 0x002081B3u;

// sw x2, 8(x1)  →  rs1=1, rs2=2, imm=8
static constexpr uint32_t SW_X2_8_X1 = 0x0020A423u;

static IfId make_fetch(uint32_t raw, uint32_t pc = PC) {
    return IfId{pc, raw, true};
}

// ── Basic decode ──────────────────────────────────────────────────────────────

TEST(DecodeStage, DecodesAddi) {
    RegFile rf;
    Latch<IfId>  in;  Latch<IdEx> out;

    in.write(make_fetch(ADDI_X1_X0_42)); in.latch();

    DecodeStage id(rf, in, out);
    id.evaluate(); id.latch();

    EXPECT_EQ(out.read().op,  Op::ADDI);
    EXPECT_EQ(out.read().rd,  1u);
    EXPECT_EQ(out.read().rs1, 0u);
    EXPECT_EQ(out.read().imm, 42);
    EXPECT_TRUE(out.read().valid);
}

TEST(DecodeStage, ReadsRegisterValues) {
    RegFile rf;
    rf.write(1, 100);
    rf.write(2, 200);

    Latch<IfId> in; Latch<IdEx> out;
    in.write(make_fetch(ADD_X3_X1_X2)); in.latch();

    DecodeStage id(rf, in, out);
    id.evaluate(); id.latch();

    EXPECT_EQ(out.read().rs1_val, 100u);
    EXPECT_EQ(out.read().rs2_val, 200u);
    EXPECT_EQ(out.read().rd,      3u);
}

TEST(DecodeStage, PcPropagated) {
    RegFile rf;
    Latch<IfId> in; Latch<IdEx> out;
    in.write(make_fetch(ADDI_X1_X0_42, 0x80000010)); in.latch();

    DecodeStage id(rf, in, out);
    id.evaluate(); id.latch();

    EXPECT_EQ(out.read().pc, 0x80000010u);
}

TEST(DecodeStage, StoreImmediateExtracted) {
    RegFile rf;
    rf.write(1, 0xABCD);
    rf.write(2, 0x1234);

    Latch<IfId> in; Latch<IdEx> out;
    in.write(make_fetch(SW_X2_8_X1)); in.latch();

    DecodeStage id(rf, in, out);
    id.evaluate(); id.latch();

    EXPECT_EQ(out.read().op,      Op::SW);
    EXPECT_EQ(out.read().imm,     8);
    EXPECT_EQ(out.read().rs1_val, 0xABCDu);
    EXPECT_EQ(out.read().rs2_val, 0x1234u);
}

// ── Bubble propagation ────────────────────────────────────────────────────────

TEST(DecodeStage, UpstreamBubbleProducesInvalidOutput) {
    RegFile rf;
    Latch<IfId> in; Latch<IdEx> out;
    // upstream latch left at default (valid=false)

    DecodeStage id(rf, in, out);
    id.evaluate(); id.latch();

    EXPECT_FALSE(out.read().valid);
}

// ── Stall ─────────────────────────────────────────────────────────────────────

TEST(DecodeStage, StallHoldsOutput) {
    RegFile rf;
    Latch<IfId> in; Latch<IdEx> out;
    in.write(make_fetch(ADDI_X1_X0_42)); in.latch();

    DecodeStage id(rf, in, out);
    id.evaluate(); id.latch();               // cycle 1: commit ADDI

    // cycle 2: stall — IdEx must hold previous value
    id.set_stall(true);
    in.write(IfId{});                        // upstream sends bubble
    in.latch();
    id.evaluate(); id.latch();

    EXPECT_EQ(out.read().op,   Op::ADDI);
    EXPECT_TRUE(out.read().valid);
}

TEST(DecodeStage, StallAutoResets) {
    RegFile rf;
    Latch<IfId> in; Latch<IdEx> out;
    in.write(make_fetch(ADDI_X1_X0_42)); in.latch();

    DecodeStage id(rf, in, out);
    id.set_stall(true);
    id.evaluate(); id.latch();               // stalled

    in.write(make_fetch(ADD_X3_X1_X2)); in.latch();
    id.evaluate(); id.latch();               // stall cleared automatically

    EXPECT_EQ(out.read().op, Op::ADD);
}

// ── Flush ─────────────────────────────────────────────────────────────────────

TEST(DecodeStage, FlushInsertsBubble) {
    RegFile rf;
    Latch<IfId> in; Latch<IdEx> out;
    in.write(make_fetch(ADDI_X1_X0_42)); in.latch();

    DecodeStage id(rf, in, out);
    id.evaluate(); id.latch();               // cycle 1: commit ADDI

    id.set_flush(true);
    id.evaluate(); id.latch();

    EXPECT_FALSE(out.read().valid);
}

TEST(DecodeStage, FlushAutoResets) {
    RegFile rf;
    Latch<IfId> in; Latch<IdEx> out;
    in.write(make_fetch(ADDI_X1_X0_42)); in.latch();

    DecodeStage id(rf, in, out);
    id.set_flush(true);
    id.evaluate(); id.latch();               // flushed

    in.write(make_fetch(ADD_X3_X1_X2)); in.latch();
    id.evaluate(); id.latch();               // flush cleared automatically

    EXPECT_EQ(out.read().op, Op::ADD);
}
