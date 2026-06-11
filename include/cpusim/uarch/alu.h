#pragma once
#include <cstdint>
#include "cpusim/isa/rv32i/decoder.h"

// Pure combinational ALU and branch comparator.
//
// In the 5-stage pipeline, EX stage has two separate units:
//   - ALU        : arithmetic/logic + effective addresses
//   - Comparator : branch conditions (runs in parallel with ALU)
//
// Neither has state.

namespace cpusim {

// ALU operations — ISA-agnostic.
// Many instructions share the same AluOp (e.g. ADD, ADDI,
// LW, SW, AUIPC all map to AluOp::ADD).
enum class AluOp : uint8_t {
    ADD,     // a + b
    SUB,     // a - b
    AND,     // a & b
    OR,      // a | b
    XOR,     // a ^ b
    SLL,     // a << (b & 0x1f)  logical left
    SRL,     // a >> (b & 0x1f)  logical right (zero-fill)
    SRA,     // a >> (b & 0x1f)  arithmetic (sign-fill)
    SLT,     // signed(a) < signed(b) ? 1 : 0
    SLTU,    // a < b ? 1 : 0  (unsigned)
    PASS_B,  // output = b  (LUI: rd <- imm)
};

// Execute one ALU operation. No side effects.
uint32_t alu_exec(AluOp op, uint32_t a, uint32_t b);

// Map an instruction Op to the AluOp for the EX stage.
//   Loads/stores/JALR/AUIPC → ADD  (base + offset)
//   LUI                     → PASS_B
//   Branches/JAL            → ADD  (target is PC±imm, not ALU output)
AluOp alu_op_for(rv32i::Op op);

// Branch comparator — separate unit in real hardware.
// Returns true when the branch condition is satisfied.
// Returns false for any non-branch Op.
bool branch_taken(rv32i::Op op, uint32_t rs1, uint32_t rs2);

}  // namespace cpusim
