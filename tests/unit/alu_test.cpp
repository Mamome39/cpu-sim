#include <gtest/gtest.h>
#include "cpusim/uarch/alu.h"

using namespace cpusim;
using rv32i::Op;

// ── alu_exec ─────────────────────────────────────────────────────────────────

TEST(Alu, Add) {
    EXPECT_EQ(alu_exec(AluOp::ADD, 5, 3), 8u);
}

TEST(Alu, AddWrapsAround) {
    // unsigned wrap: 0xFFFFFFFF + 1 = 0
    EXPECT_EQ(alu_exec(AluOp::ADD, 0xFFFFFFFF, 1), 0u);
}

TEST(Alu, Sub) {
    EXPECT_EQ(alu_exec(AluOp::SUB, 5, 3), 2u);
}

TEST(Alu, SubWrapsAround) {
    // 0 - 1 wraps to 0xFFFFFFFF
    EXPECT_EQ(alu_exec(AluOp::SUB, 0, 1), 0xFFFFFFFFu);
}

TEST(Alu, And) {
    EXPECT_EQ(alu_exec(AluOp::AND, 0xFF0F, 0x0FFF), 0x0F0Fu);
}

TEST(Alu, Or) {
    EXPECT_EQ(alu_exec(AluOp::OR, 0xFF00, 0x00FF), 0xFFFFu);
}

TEST(Alu, Xor) {
    EXPECT_EQ(alu_exec(AluOp::XOR, 0xFFFF, 0x0F0F), 0xF0F0u);
}

TEST(Alu, Sll) {
    EXPECT_EQ(alu_exec(AluOp::SLL, 1, 4), 16u);
}

TEST(Alu, SllUsesLower5BitsOfShamt) {
    // shamt = 32 & 0x1f = 0 → no shift
    EXPECT_EQ(alu_exec(AluOp::SLL, 1, 32), 1u);
}

TEST(Alu, Srl) {
    // logical: high bit must be 0-filled
    EXPECT_EQ(alu_exec(AluOp::SRL, 0x80000000u, 1), 0x40000000u);
}

TEST(Alu, Sra) {
    // arithmetic: high bit must be sign-filled
    EXPECT_EQ(alu_exec(AluOp::SRA, 0x80000000u, 1), 0xC0000000u);
}

TEST(Alu, SraPositive) {
    // positive value: same as SRL
    EXPECT_EQ(alu_exec(AluOp::SRA, 16, 2), 4u);
}

TEST(Alu, Slt_Taken) {
    // -1 < 1 (signed)
    EXPECT_EQ(alu_exec(AluOp::SLT, 0xFFFFFFFFu, 1), 1u);
}

TEST(Alu, Slt_NotTaken) {
    EXPECT_EQ(alu_exec(AluOp::SLT, 1, 1), 0u);
}

TEST(Alu, Sltu_Taken) {
    // 0xFFFFFFFF is a large unsigned number > 1
    EXPECT_EQ(alu_exec(AluOp::SLTU, 1, 0xFFFFFFFFu), 1u);
}

TEST(Alu, Sltu_NotTaken) {
    // 0xFFFFFFFF > 1 unsigned → not less than
    EXPECT_EQ(alu_exec(AluOp::SLTU, 0xFFFFFFFFu, 1), 0u);
}

TEST(Alu, PassB) {
    // LUI: a is ignored, output is b
    EXPECT_EQ(alu_exec(AluOp::PASS_B, 0xDEAD, 0x12345000u), 0x12345000u);
}

// ── alu_op_for ───────────────────────────────────────────────────────────────

TEST(AluOpFor, ArithmeticOps) {
    EXPECT_EQ(alu_op_for(Op::ADD),   AluOp::ADD);
    EXPECT_EQ(alu_op_for(Op::ADDI),  AluOp::ADD);
    EXPECT_EQ(alu_op_for(Op::SUB),   AluOp::SUB);
    EXPECT_EQ(alu_op_for(Op::AND),   AluOp::AND);
    EXPECT_EQ(alu_op_for(Op::ANDI),  AluOp::AND);
    EXPECT_EQ(alu_op_for(Op::OR),    AluOp::OR);
    EXPECT_EQ(alu_op_for(Op::XOR),   AluOp::XOR);
    EXPECT_EQ(alu_op_for(Op::SLL),   AluOp::SLL);
    EXPECT_EQ(alu_op_for(Op::SLLI),  AluOp::SLL);
    EXPECT_EQ(alu_op_for(Op::SRL),   AluOp::SRL);
    EXPECT_EQ(alu_op_for(Op::SRA),   AluOp::SRA);
    EXPECT_EQ(alu_op_for(Op::SRAI),  AluOp::SRA);
    EXPECT_EQ(alu_op_for(Op::SLT),   AluOp::SLT);
    EXPECT_EQ(alu_op_for(Op::SLTI),  AluOp::SLT);
    EXPECT_EQ(alu_op_for(Op::SLTU),  AluOp::SLTU);
    EXPECT_EQ(alu_op_for(Op::SLTIU), AluOp::SLTU);
}

TEST(AluOpFor, MemoryOpsUseAdd) {
    // All loads and stores compute address = base + offset
    EXPECT_EQ(alu_op_for(Op::LW),  AluOp::ADD);
    EXPECT_EQ(alu_op_for(Op::LB),  AluOp::ADD);
    EXPECT_EQ(alu_op_for(Op::LBU), AluOp::ADD);
    EXPECT_EQ(alu_op_for(Op::SW),  AluOp::ADD);
    EXPECT_EQ(alu_op_for(Op::SB),  AluOp::ADD);
}

TEST(AluOpFor, LuiUsesPassB) {
    EXPECT_EQ(alu_op_for(Op::LUI), AluOp::PASS_B);
}

TEST(AluOpFor, AuipcUsesAdd) {
    // AUIPC: result = PC + imm, both fed into ALU
    EXPECT_EQ(alu_op_for(Op::AUIPC), AluOp::ADD);
}

// ── branch_taken ─────────────────────────────────────────────────────────────

TEST(BranchTaken, Beq) {
    EXPECT_TRUE (branch_taken(Op::BEQ, 7, 7));
    EXPECT_FALSE(branch_taken(Op::BEQ, 7, 8));
}

TEST(BranchTaken, Bne) {
    EXPECT_TRUE (branch_taken(Op::BNE, 7, 8));
    EXPECT_FALSE(branch_taken(Op::BNE, 7, 7));
}

TEST(BranchTaken, Blt_Signed) {
    EXPECT_TRUE (branch_taken(Op::BLT, 0xFFFFFFFFu, 1)); // -1 < 1
    EXPECT_FALSE(branch_taken(Op::BLT, 1, 1));
    EXPECT_FALSE(branch_taken(Op::BLT, 2, 1));
}

TEST(BranchTaken, Bge_Signed) {
    EXPECT_TRUE (branch_taken(Op::BGE, 1, 1));
    EXPECT_TRUE (branch_taken(Op::BGE, 2, 1));
    EXPECT_FALSE(branch_taken(Op::BGE, 0xFFFFFFFFu, 1)); // -1 < 1
}

TEST(BranchTaken, Bltu_Unsigned) {
    // 0xFFFFFFFF is large unsigned, not less than 1
    EXPECT_FALSE(branch_taken(Op::BLTU, 0xFFFFFFFFu, 1));
    EXPECT_TRUE (branch_taken(Op::BLTU, 1, 0xFFFFFFFFu));
}

TEST(BranchTaken, Bgeu_Unsigned) {
    EXPECT_TRUE (branch_taken(Op::BGEU, 0xFFFFFFFFu, 1));
    EXPECT_FALSE(branch_taken(Op::BGEU, 1, 0xFFFFFFFFu));
}

TEST(BranchTaken, NonBranchReturnsFalse) {
    EXPECT_FALSE(branch_taken(Op::ADD, 1, 1));
    EXPECT_FALSE(branch_taken(Op::LW,  0, 0));
}
