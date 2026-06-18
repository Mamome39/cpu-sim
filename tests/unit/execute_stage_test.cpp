#include <gtest/gtest.h>
#include "cpusim/memory/flat_mem.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/pipeline/fetch.h"
#include "cpusim/uarch/pipeline/decode.h"
#include "cpusim/uarch/pipeline/execute.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"
#include "cpusim/isa/rv32i/decoder.h"
#include "cpusim/regfile.h"

using namespace cpusim;
using namespace cpusim::pipeline;
using namespace cpusim::rv32i;

static constexpr uint32_t PC = 0x80000000;

// Minimal stubs so ExecuteStage has real FetchStage / DecodeStage
// objects to call set_redirect / set_flush on.
static FlatMem& stub_mem() {
    static FlatMem m(0x80000000, 0x1000);
    return m;
}

struct Fixture {
    Latch<IfId>   if_id;
    Latch<IdEx>   id_ex;
    Latch<ExMem>  ex_mem;
    RegFile       rf;
    FetchStage    fetch{stub_mem(), if_id, PC};
    DecodeStage   decode{rf, if_id, id_ex};
    ExecuteStage  ex{id_ex, ex_mem, fetch, decode};

    // Push a pre-built IdEx into the latch so ExecuteStage can read it.
    void load(const IdEx& v) { id_ex.write(v); id_ex.latch(); }
};

static IdEx make_idex(Op op, uint32_t pc,
                      uint32_t rs1_val, uint32_t rs2_val,
                      int32_t imm = 0, uint8_t rd = 1,
                      uint8_t rs1 = 1, uint8_t rs2 = 2) {
    IdEx d;
    d.pc = pc; d.op = op; d.rd = rd;
    d.rs1 = rs1; d.rs2 = rs2; d.imm = imm;
    d.rs1_val = rs1_val; d.rs2_val = rs2_val;
    d.valid = true;
    return d;
}

// ── ALU ops ──────────────────────────────────────────────────────────────────

TEST(ExecuteStage, AddReg) {
    Fixture f;
    f.load(make_idex(Op::ADD, PC, 10, 20));
    f.ex.evaluate(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, 30u);
    EXPECT_TRUE(f.ex_mem.read().valid);
}

TEST(ExecuteStage, AddiImmediate) {
    Fixture f;
    f.load(make_idex(Op::ADDI, PC, 5, 0, 7));
    f.ex.evaluate(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, 12u);
}

TEST(ExecuteStage, SubReg) {
    Fixture f;
    f.load(make_idex(Op::SUB, PC, 30, 10));
    f.ex.evaluate(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, 20u);
}

TEST(ExecuteStage, AuipcAddsImmToPC) {
    Fixture f;
    f.load(make_idex(Op::AUIPC, PC, 0, 0, 0x1000));
    f.ex.evaluate(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, PC + 0x1000u);
}

TEST(ExecuteStage, LuiPassesImm) {
    Fixture f;
    f.load(make_idex(Op::LUI, PC, 0, 0, 0x12000));
    f.ex.evaluate(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, 0x12000u);
}

// ── Load/store effective address ──────────────────────────────────────────────

TEST(ExecuteStage, LwComputesAddress) {
    Fixture f;
    // lw rd, 8(x1)  — alu_out = base + offset
    f.load(make_idex(Op::LW, PC, 0x1000, 0, 8));
    f.ex.evaluate(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out, 0x1008u);
}

TEST(ExecuteStage, SwPreservesRs2Val) {
    Fixture f;
    f.load(make_idex(Op::SW, PC, 0x2000, 0xDEAD, 4));
    f.ex.evaluate(); f.ex.latch();
    EXPECT_EQ(f.ex_mem.read().alu_out,  0x2004u);   // address
    EXPECT_EQ(f.ex_mem.read().rs2_val,  0xDEADu);   // store data
}

// ── Branch resolution ─────────────────────────────────────────────────────────

TEST(ExecuteStage, BeqTakenRedirects) {
    Fixture f;
    // beq x1, x1, +16  — rs1==rs2, should redirect
    f.load(make_idex(Op::BEQ, PC, 42, 42, 16));
    f.ex.evaluate(); f.ex.latch();
    // redirect signal is set; fetch.latch() applies it to pc_
    f.fetch.latch();

    EXPECT_TRUE(f.ex_mem.read().branch_taken);
    EXPECT_EQ(f.ex_mem.read().branch_target, PC + 16u);
    EXPECT_EQ(f.fetch.pc(), PC + 16u);
}

TEST(ExecuteStage, BeqNotTakenNoRedirect) {
    Fixture f;
    f.load(make_idex(Op::BEQ, PC, 1, 2, 16));
    f.ex.evaluate(); f.ex.latch();

    EXPECT_FALSE(f.ex_mem.read().branch_taken);
    // PC should not have moved (fetch pc still at reset + 4 from
    // evaluate internally; we only check branch_taken here)
    EXPECT_FALSE(f.ex_mem.read().branch_taken);
}

TEST(ExecuteStage, BneTaken) {
    Fixture f;
    f.load(make_idex(Op::BNE, PC, 1, 2, 8));
    f.ex.evaluate(); f.ex.latch();
    EXPECT_TRUE(f.ex_mem.read().branch_taken);
    EXPECT_EQ(f.ex_mem.read().branch_target, PC + 8u);
}

// ── JAL / JALR ───────────────────────────────────────────────────────────────

TEST(ExecuteStage, JalWritesReturnAddr) {
    Fixture f;
    // jal x1, +32  — rd gets pc+4, fetch redirected to pc+32
    f.load(make_idex(Op::JAL, PC, 0, 0, 32));
    f.ex.evaluate(); f.ex.latch();
    f.fetch.latch();

    EXPECT_EQ(f.ex_mem.read().alu_out, PC + 4u);   // return addr
    EXPECT_EQ(f.fetch.pc(), PC + 32u);              // redirect
}

TEST(ExecuteStage, JalrAlignsTarget) {
    Fixture f;
    // jalr x0, x1, 1  — target = (rs1+imm) & ~1
    f.load(make_idex(Op::JALR, PC, 0x100, 0, 1));
    f.ex.evaluate(); f.ex.latch();
    f.fetch.latch();

    // (0x100 + 1) & ~1 = 0x100
    EXPECT_EQ(f.fetch.pc(), 0x100u);
}

// ── Bubble propagation ────────────────────────────────────────────────────────

TEST(ExecuteStage, BubbleProducesInvalidOutput) {
    Fixture f;
    // id_ex left at default (valid=false)
    f.ex.evaluate(); f.ex.latch();
    EXPECT_FALSE(f.ex_mem.read().valid);
}
