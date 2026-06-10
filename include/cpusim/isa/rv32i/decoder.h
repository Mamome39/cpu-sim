#pragma once
#include <cstdint>

namespace cpusim::rv32i {

// Every RV32I instruction maps to one Op.
// ILLEGAL covers any encoding that doesn't match a valid instruction.
enum class Op : uint8_t {
    // R-type
    ADD, SUB,
    SLL, SRL, SRA,
    SLT, SLTU,
    AND, OR, XOR,

    // I-type arithmetic
    ADDI,
    SLLI, SRLI, SRAI,
    SLTI, SLTIU,
    ANDI, ORI, XORI,

    // Loads
    LB, LH, LW, LBU, LHU,

    // Stores
    SB, SH, SW,

    // Branches
    BEQ, BNE,
    BLT, BGE,
    BLTU, BGEU,

    // Jumps
    JAL, JALR,

    // Upper immediates
    LUI, AUIPC,

    // System / misc
    ECALL, EBREAK, FENCE,

    ILLEGAL,
};

// Decoded representation of one RV32I instruction.
// The pipeline carries pc separately in each latch; it is passed in here
// only so the decoder can be used standalone (e.g. in tests and the tracer).
struct Instruction {
    uint32_t raw;    // original 32-bit encoding
    uint32_t pc;     // address this instruction was fetched from
    Op       op;
    uint8_t  rd;
    uint8_t  rs1;
    uint8_t  rs2;
    int32_t  imm;    // sign-extended immediate (0 for R-type)
};

// Decode a 32-bit instruction word into an Instruction.
// Returns op == Op::ILLEGAL for any unrecognised encoding.
Instruction decode(uint32_t raw, uint32_t pc = 0);

// Human-readable name for an Op — useful in traces and asserts.
const char* op_name(Op op);

}  // namespace cpusim::rv32i
