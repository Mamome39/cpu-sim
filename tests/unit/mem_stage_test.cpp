#include <gtest/gtest.h>
#include "cpusim/memory/flat_mem.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/pipeline/mem_access.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/isa/rv32i/decoder.h"

using namespace cpusim;
using namespace cpusim::pipeline;
using namespace cpusim::rv32i;

static constexpr uint32_t BASE = 0x80000000;
static constexpr size_t   SZ   = 0x1000;
static constexpr uint32_t PC   = 0x80000000;

static ExMem make_exmem(Op op, uint32_t alu_out,
                        uint32_t rs2_val = 0, uint8_t rd = 1) {
    ExMem e;
    e.pc = PC; e.op = op; e.rd = rd;
    e.alu_out = alu_out; e.rs2_val = rs2_val;
    e.valid = true;
    return e;
}

struct Fixture {
    FlatMem         dmem{BASE, SZ};
    Latch<ExMem>    in;
    Latch<MemWb>    out;
    MemAccessStage  mem{dmem, in, out};

    void load(const ExMem& v) { in.write(v); in.latch(); }
};

// ── Load word ─────────────────────────────────────────────────────────────────

TEST(MemStage, LoadWord) {
    Fixture f;
    f.dmem.store_word(BASE + 8, 0xDEADBEEFu);
    f.load(make_exmem(Op::LW, BASE + 8));
    f.mem.evaluate(); f.mem.latch();
    EXPECT_EQ(f.out.read().wb_val, 0xDEADBEEFu);
    EXPECT_TRUE(f.out.read().valid);
}

// ── Load half — signed ────────────────────────────────────────────────────────

TEST(MemStage, LoadHalfSigned) {
    Fixture f;
    f.dmem.store_half(BASE, 0x8001u);   // negative when sign-extended
    f.load(make_exmem(Op::LH, BASE));
    f.mem.evaluate(); f.mem.latch();
    // 0x8001 sign-extended to 32 bits = 0xFFFF8001
    EXPECT_EQ(f.out.read().wb_val, 0xFFFF8001u);
}

TEST(MemStage, LoadHalfUnsigned) {
    Fixture f;
    f.dmem.store_half(BASE, 0x8001u);
    f.load(make_exmem(Op::LHU, BASE));
    f.mem.evaluate(); f.mem.latch();
    EXPECT_EQ(f.out.read().wb_val, 0x8001u);
}

// ── Load byte — signed ────────────────────────────────────────────────────────

TEST(MemStage, LoadByteSigned) {
    Fixture f;
    f.dmem.store_byte(BASE, 0x80u);    // -128 signed
    f.load(make_exmem(Op::LB, BASE));
    f.mem.evaluate(); f.mem.latch();
    EXPECT_EQ(f.out.read().wb_val, 0xFFFFFF80u);
}

TEST(MemStage, LoadByteUnsigned) {
    Fixture f;
    f.dmem.store_byte(BASE, 0x80u);
    f.load(make_exmem(Op::LBU, BASE));
    f.mem.evaluate(); f.mem.latch();
    EXPECT_EQ(f.out.read().wb_val, 0x80u);
}

// ── Stores ────────────────────────────────────────────────────────────────────

TEST(MemStage, StoreWord) {
    Fixture f;
    f.load(make_exmem(Op::SW, BASE + 4, 0x12345678u));
    f.mem.evaluate(); f.mem.latch();
    EXPECT_EQ(f.dmem.load_word(BASE + 4), 0x12345678u);
}

TEST(MemStage, StoreHalf) {
    Fixture f;
    f.load(make_exmem(Op::SH, BASE, 0xABCDu));
    f.mem.evaluate(); f.mem.latch();
    EXPECT_EQ(f.dmem.load_half(BASE), 0xABCDu);
}

TEST(MemStage, StoreByte) {
    Fixture f;
    f.load(make_exmem(Op::SB, BASE, 0xFFu));
    f.mem.evaluate(); f.mem.latch();
    EXPECT_EQ(f.dmem.load_byte(BASE), 0xFFu);
}

// ── Non-memory op passes alu_out through ─────────────────────────────────────

TEST(MemStage, AluResultPassThrough) {
    Fixture f;
    f.load(make_exmem(Op::ADD, 0xCAFEu));
    f.mem.evaluate(); f.mem.latch();
    EXPECT_EQ(f.out.read().wb_val, 0xCAFEu);
}

// ── Bubble propagation ────────────────────────────────────────────────────────

TEST(MemStage, BubbleProducesInvalidOutput) {
    Fixture f;
    // in left at default (valid=false)
    f.mem.evaluate(); f.mem.latch();
    EXPECT_FALSE(f.out.read().valid);
}

// ── rd and pc propagated ──────────────────────────────────────────────────────

TEST(MemStage, MetadataPropagated) {
    Fixture f;
    ExMem e = make_exmem(Op::ADDI, 99u, 0, 5);
    e.pc = 0x80000010;
    f.load(e);
    f.mem.evaluate(); f.mem.latch();
    EXPECT_EQ(f.out.read().rd, 5u);
    EXPECT_EQ(f.out.read().pc, 0x80000010u);
}
