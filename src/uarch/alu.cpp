#include "cpusim/uarch/alu.h"

namespace cpusim {

uint32_t alu_exec(AluOp op, uint32_t a, uint32_t b) {
    switch (op) {
    case AluOp::ADD:    return a + b;
    case AluOp::SUB:    return a - b;
    case AluOp::AND:    return a & b;
    case AluOp::OR:     return a | b;
    case AluOp::XOR:    return a ^ b;
    case AluOp::SLL:    return a << (b & 0x1fu);
    case AluOp::SRL:    return a >> (b & 0x1fu);
    case AluOp::SRA:    return static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 0x1fu));
    case AluOp::SLT:    return static_cast<int32_t>(a) < static_cast<int32_t>(b) ? 1u : 0u;
    case AluOp::SLTU:   return a < b ? 1u : 0u;
    case AluOp::PASS_B: return b;
    }
    return 0u;
}

AluOp alu_op_for(rv32i::Op op) {
    using rv32i::Op;
    switch (op) {
    // Arithmetic
    case Op::ADD:  case Op::ADDI:  return AluOp::ADD;
    case Op::SUB:                  return AluOp::SUB;
    case Op::AND:  case Op::ANDI:  return AluOp::AND;
    case Op::OR:   case Op::ORI:   return AluOp::OR;
    case Op::XOR:  case Op::XORI:  return AluOp::XOR;
    case Op::SLL:  case Op::SLLI:  return AluOp::SLL;
    case Op::SRL:  case Op::SRLI:  return AluOp::SRL;
    case Op::SRA:  case Op::SRAI:  return AluOp::SRA;
    case Op::SLT:  case Op::SLTI:  return AluOp::SLT;
    case Op::SLTU: case Op::SLTIU: return AluOp::SLTU;
    // Upper immediate
    case Op::LUI:                  return AluOp::PASS_B;
    // Effective address = base + offset (loads, stores, JALR, AUIPC use ADD)
    case Op::LB:  case Op::LH:  case Op::LW:
    case Op::LBU: case Op::LHU:
    case Op::SB:  case Op::SH:  case Op::SW:
    case Op::JALR:
    case Op::AUIPC:
        return AluOp::ADD;
    // JAL / branches don't produce an ALU result used by writeback
    default:
        return AluOp::ADD;
    }
}

bool branch_taken(rv32i::Op op, uint32_t rs1, uint32_t rs2) {
    using rv32i::Op;
    switch (op) {
    case Op::BEQ:  return rs1 == rs2;
    case Op::BNE:  return rs1 != rs2;
    case Op::BLT:  return static_cast<int32_t>(rs1) <  static_cast<int32_t>(rs2);
    case Op::BGE:  return static_cast<int32_t>(rs1) >= static_cast<int32_t>(rs2);
    case Op::BLTU: return rs1 <  rs2;
    case Op::BGEU: return rs1 >= rs2;
    default:       return false;
    }
}

}  // namespace cpusim
