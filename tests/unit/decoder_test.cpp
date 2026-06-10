#include <gtest/gtest.h>
#include "cpusim/isa/rv32i/decoder.h"
#include "cpusim/isa/rv32i/encoding.h"

using namespace cpusim::rv32i;
using namespace cpusim::rv32i::enc;

// ── Encoding helpers ──────────────────────────────────────────────────────────
// Test that the bit-field extraction functions produce correct values on a
// known R-type encoding: add x3, x1, x2  →  0x002081B3
//   funct7=0  rs2=2  rs1=1  funct3=0  rd=3  opcode=0x33

TEST(Encoding, FieldExtraction) {
    constexpr uint32_t w = 0x002081B3;
    EXPECT_EQ(opcode(w), 0x33u);   // OPC_OP
    EXPECT_EQ(rd(w),     3u);
    EXPECT_EQ(funct3(w), 0u);
    EXPECT_EQ(rs1(w),    1u);
    EXPECT_EQ(rs2(w),    2u);
    EXPECT_EQ(funct7(w), 0u);
}

TEST(Encoding, ImmI_Positive) {
    // addi x1, x2, 42  →  0x02A10093
    EXPECT_EQ(imm_i(0x02A10093), 42);
}

TEST(Encoding, ImmI_Negative) {
    // addi x1, x0, -5  →  0xFFB00093
    EXPECT_EQ(imm_i(0xFFB00093), -5);
}

TEST(Encoding, ImmS_Positive) {
    // sw x1, 8(x2)  →  0x00112423   imm[11:5]=0  imm[4:0]=8
    EXPECT_EQ(imm_s(0x00112423), 8);
}

TEST(Encoding, ImmS_Negative) {
    // sw x1, -4(x2)  →  0xFE112E23
    EXPECT_EQ(imm_s(0xFE112E23), -4);
}

TEST(Encoding, ImmB_Positive) {
    // beq x1, x2, +8  →  0x00208463
    EXPECT_EQ(imm_b(0x00208463), 8);
}

TEST(Encoding, ImmB_Negative) {
    // beq x1, x2, -8  →  0xFE208CE3
    EXPECT_EQ(imm_b(0xFE208CE3), -8);
}

TEST(Encoding, ImmU) {
    // lui x1, 0x12345  →  0x123450B7   imm = upper 20 bits << 12
    EXPECT_EQ(imm_u(0x123450B7), (int32_t)0x12345000u);
}

TEST(Encoding, ImmJ_Positive) {
    // jal x1, +8  →  0x008000EF
    EXPECT_EQ(imm_j(0x008000EF), 8);
}

TEST(Encoding, ImmJ_Negative) {
    // jal x0, -4  →  0xFFDFF06F
    EXPECT_EQ(imm_j(0xFFDFF06F), -4);
}

// ── Decoder — R-type ─────────────────────────────────────────────────────────

TEST(Decoder, ADD) {
    auto i = decode(0x002081B3);  // add x3, x1, x2
    EXPECT_EQ(i.op,  Op::ADD);
    EXPECT_EQ(i.rd,  3u); EXPECT_EQ(i.rs1, 1u); EXPECT_EQ(i.rs2, 2u);
    EXPECT_EQ(i.imm, 0);
}

TEST(Decoder, SUB) {
    auto i = decode(0x402081B3);  // sub x3, x1, x2
    EXPECT_EQ(i.op,  Op::SUB);
    EXPECT_EQ(i.rd,  3u); EXPECT_EQ(i.rs1, 1u); EXPECT_EQ(i.rs2, 2u);
}

TEST(Decoder, AND) {
    auto i = decode(0x0020F1B3);  // and x3, x1, x2
    EXPECT_EQ(i.op, Op::AND);
    EXPECT_EQ(i.rd, 3u); EXPECT_EQ(i.rs1, 1u); EXPECT_EQ(i.rs2, 2u);
}

TEST(Decoder, OR) {
    auto i = decode(0x0020E1B3);  // or x3, x1, x2
    EXPECT_EQ(i.op, Op::OR);
}

// ── Decoder — I-type arithmetic ───────────────────────────────────────────────

TEST(Decoder, ADDI_Positive) {
    auto i = decode(0x02A10093);  // addi x1, x2, 42
    EXPECT_EQ(i.op,  Op::ADDI);
    EXPECT_EQ(i.rd,  1u); EXPECT_EQ(i.rs1, 2u); EXPECT_EQ(i.rs2, 0u);
    EXPECT_EQ(i.imm, 42);
}

TEST(Decoder, ADDI_Negative) {
    auto i = decode(0xFFB00093);  // addi x1, x0, -5
    EXPECT_EQ(i.op,  Op::ADDI);
    EXPECT_EQ(i.rd,  1u); EXPECT_EQ(i.rs1, 0u);
    EXPECT_EQ(i.imm, -5);
}

TEST(Decoder, SLLI) {
    auto i = decode(0x00331293);  // slli x5, x6, 3
    EXPECT_EQ(i.op,  Op::SLLI);
    EXPECT_EQ(i.rd,  5u); EXPECT_EQ(i.rs1, 6u); EXPECT_EQ(i.rs2, 0u);
    EXPECT_EQ(i.imm, 3);  // shamt only
}

TEST(Decoder, SRLI) {
    auto i = decode(0x00335293);  // srli x5, x6, 3
    EXPECT_EQ(i.op,  Op::SRLI);
    EXPECT_EQ(i.imm, 3);
}

TEST(Decoder, SRAI) {
    auto i = decode(0x40335293);  // srai x5, x6, 3
    EXPECT_EQ(i.op,  Op::SRAI);
    EXPECT_EQ(i.imm, 3);  // shamt only, funct7 not leaked into imm
}

// ── Decoder — Loads ───────────────────────────────────────────────────────────

TEST(Decoder, LW) {
    auto i = decode(0x00412083);  // lw x1, 4(x2)
    EXPECT_EQ(i.op,  Op::LW);
    EXPECT_EQ(i.rd,  1u); EXPECT_EQ(i.rs1, 2u); EXPECT_EQ(i.rs2, 0u);
    EXPECT_EQ(i.imm, 4);
}

TEST(Decoder, LBU) {
    auto i = decode(0x00C34283);  // lbu x5, 12(x6)
    EXPECT_EQ(i.op,  Op::LBU);
    EXPECT_EQ(i.rd,  5u); EXPECT_EQ(i.rs1, 6u);
    EXPECT_EQ(i.imm, 12);
}

// ── Decoder — Stores ──────────────────────────────────────────────────────────

TEST(Decoder, SW_Positive) {
    auto i = decode(0x00112423);  // sw x1, 8(x2)
    EXPECT_EQ(i.op,  Op::SW);
    EXPECT_EQ(i.rd,  0u); EXPECT_EQ(i.rs1, 2u); EXPECT_EQ(i.rs2, 1u);
    EXPECT_EQ(i.imm, 8);
}

TEST(Decoder, SW_Negative) {
    auto i = decode(0xFE112E23);  // sw x1, -4(x2)
    EXPECT_EQ(i.op,  Op::SW);
    EXPECT_EQ(i.rs1, 2u); EXPECT_EQ(i.rs2, 1u);
    EXPECT_EQ(i.imm, -4);
}

TEST(Decoder, SB) {
    auto i = decode(0x001101A3);  // sb x1, 3(x2)
    EXPECT_EQ(i.op,  Op::SB);
    EXPECT_EQ(i.rs1, 2u); EXPECT_EQ(i.rs2, 1u);
    EXPECT_EQ(i.imm, 3);
}

// ── Decoder — Branches ────────────────────────────────────────────────────────

TEST(Decoder, BEQ_Positive) {
    auto i = decode(0x00208463);  // beq x1, x2, +8
    EXPECT_EQ(i.op,  Op::BEQ);
    EXPECT_EQ(i.rd,  0u); EXPECT_EQ(i.rs1, 1u); EXPECT_EQ(i.rs2, 2u);
    EXPECT_EQ(i.imm, 8);
}

TEST(Decoder, BNE) {
    auto i = decode(0x00209463);  // bne x1, x2, +8
    EXPECT_EQ(i.op,  Op::BNE);
    EXPECT_EQ(i.imm, 8);
}

TEST(Decoder, BLT) {
    auto i = decode(0x0020C463);  // blt x1, x2, +8
    EXPECT_EQ(i.op,  Op::BLT);
    EXPECT_EQ(i.imm, 8);
}

TEST(Decoder, BGE) {
    auto i = decode(0x0020D463);  // bge x1, x2, +8
    EXPECT_EQ(i.op,  Op::BGE);
}

TEST(Decoder, BLTU) {
    auto i = decode(0x0020E463);  // bltu x1, x2, +8
    EXPECT_EQ(i.op,  Op::BLTU);
}

TEST(Decoder, BGEU) {
    auto i = decode(0x0020F463);  // bgeu x1, x2, +8
    EXPECT_EQ(i.op,  Op::BGEU);
}

TEST(Decoder, BEQ_NegativeOffset) {
    auto i = decode(0xFE208CE3);  // beq x1, x2, -8
    EXPECT_EQ(i.op,  Op::BEQ);
    EXPECT_EQ(i.imm, -8);
}

// ── Decoder — Jumps ───────────────────────────────────────────────────────────

TEST(Decoder, JAL) {
    auto i = decode(0x008000EF);  // jal x1, +8
    EXPECT_EQ(i.op,  Op::JAL);
    EXPECT_EQ(i.rd,  1u); EXPECT_EQ(i.rs1, 0u); EXPECT_EQ(i.rs2, 0u);
    EXPECT_EQ(i.imm, 8);
}

TEST(Decoder, JAL_Negative) {
    auto i = decode(0xFFDFF06F);  // jal x0, -4
    EXPECT_EQ(i.op,  Op::JAL);
    EXPECT_EQ(i.rd,  0u);
    EXPECT_EQ(i.imm, -4);
}

TEST(Decoder, JALR) {
    auto i = decode(0x008100E7);  // jalr x1, 8(x2)
    EXPECT_EQ(i.op,  Op::JALR);
    EXPECT_EQ(i.rd,  1u); EXPECT_EQ(i.rs1, 2u); EXPECT_EQ(i.rs2, 0u);
    EXPECT_EQ(i.imm, 8);
}

// ── Decoder — Upper immediates ────────────────────────────────────────────────

TEST(Decoder, LUI) {
    auto i = decode(0x123450B7);  // lui x1, 0x12345
    EXPECT_EQ(i.op,  Op::LUI);
    EXPECT_EQ(i.rd,  1u); EXPECT_EQ(i.rs1, 0u); EXPECT_EQ(i.rs2, 0u);
    EXPECT_EQ(i.imm, (int32_t)0x12345000u);
}

TEST(Decoder, AUIPC) {
    auto i = decode(0x00001117);  // auipc x2, 0x1
    EXPECT_EQ(i.op,  Op::AUIPC);
    EXPECT_EQ(i.rd,  2u);
    EXPECT_EQ(i.imm, 0x1000);
}

// ── Decoder — System ──────────────────────────────────────────────────────────

TEST(Decoder, ECALL) {
    auto i = decode(0x00000073);
    EXPECT_EQ(i.op, Op::ECALL);
    EXPECT_EQ(i.rd, 0u); EXPECT_EQ(i.rs1, 0u); EXPECT_EQ(i.rs2, 0u);
}

TEST(Decoder, EBREAK) {
    auto i = decode(0x00100073);
    EXPECT_EQ(i.op, Op::EBREAK);
}

// ── Decoder — Illegal ─────────────────────────────────────────────────────────

TEST(Decoder, IllegalOpcode) {
    // opcode 0x7F has no valid RV32I instruction
    auto i = decode(0xFFFFFFFF);
    EXPECT_EQ(i.op, Op::ILLEGAL);
}

TEST(Decoder, IllegalFunct3InJALR) {
    // JALR with funct3=1 is reserved → ILLEGAL
    // Build jalr x0, x0, 0 but with funct3=1: opcode=0x67 | funct3=1<<12 = 0x1067
    auto i = decode(0x00001067);
    EXPECT_EQ(i.op, Op::ILLEGAL);
}

// ── Decoder — Raw + PC passthrough ────────────────────────────────────────────

TEST(Decoder, RawAndPcPreserved) {
    auto i = decode(0x002081B3, 0x80000010);
    EXPECT_EQ(i.raw, 0x002081B3u);
    EXPECT_EQ(i.pc,  0x80000010u);
}

// ── op_name ───────────────────────────────────────────────────────────────────

TEST(Decoder, OpName) {
    EXPECT_STREQ(op_name(Op::ADD),     "add");
    EXPECT_STREQ(op_name(Op::ADDI),    "addi");
    EXPECT_STREQ(op_name(Op::LW),      "lw");
    EXPECT_STREQ(op_name(Op::BEQ),     "beq");
    EXPECT_STREQ(op_name(Op::JAL),     "jal");
    EXPECT_STREQ(op_name(Op::ILLEGAL), "ILLEGAL");
}
