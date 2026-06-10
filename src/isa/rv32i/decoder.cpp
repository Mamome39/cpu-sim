#include "cpusim/isa/rv32i/decoder.h"
#include "cpusim/isa/rv32i/defs.h"
#include "cpusim/isa/rv32i/encoding.h"

namespace cpusim::rv32i {

Instruction decode(uint32_t raw, uint32_t pc) {
    using namespace enc;

    const uint32_t opc = opcode(raw);
    const uint32_t f3  = funct3(raw);
    const uint32_t f7  = funct7(raw);

    Instruction insn{};
    insn.raw = raw;
    insn.pc  = pc;
    insn.rd  = static_cast<uint8_t>(rd(raw));
    insn.rs1 = static_cast<uint8_t>(rs1(raw));
    insn.rs2 = static_cast<uint8_t>(rs2(raw));
    insn.op  = Op::ILLEGAL;

    switch (opc) {

    case OPC_LUI:
        insn.op  = Op::LUI;
        insn.imm = imm_u(raw);
        insn.rs1 = 0; insn.rs2 = 0;
        break;

    case OPC_AUIPC:
        insn.op  = Op::AUIPC;
        insn.imm = imm_u(raw);
        insn.rs1 = 0; insn.rs2 = 0;
        break;

    case OPC_JAL:
        insn.op  = Op::JAL;
        insn.imm = imm_j(raw);
        insn.rs1 = 0; insn.rs2 = 0;
        break;

    case OPC_JALR:
        if (f3 != 0b000) break;
        insn.op  = Op::JALR;
        insn.imm = imm_i(raw);
        insn.rs2 = 0;
        break;

    case OPC_BRANCH:
        insn.imm = imm_b(raw);
        insn.rd  = 0;
        switch (f3) {
        case F3_BEQ:  insn.op = Op::BEQ;  break;
        case F3_BNE:  insn.op = Op::BNE;  break;
        case F3_BLT:  insn.op = Op::BLT;  break;
        case F3_BGE:  insn.op = Op::BGE;  break;
        case F3_BLTU: insn.op = Op::BLTU; break;
        case F3_BGEU: insn.op = Op::BGEU; break;
        }
        break;

    case OPC_LOAD:
        insn.imm = imm_i(raw);
        insn.rs2 = 0;
        switch (f3) {
        case F3_LB:  insn.op = Op::LB;  break;
        case F3_LH:  insn.op = Op::LH;  break;
        case F3_LW:  insn.op = Op::LW;  break;
        case F3_LBU: insn.op = Op::LBU; break;
        case F3_LHU: insn.op = Op::LHU; break;
        }
        break;

    case OPC_STORE:
        insn.imm = imm_s(raw);
        insn.rd  = 0;
        switch (f3) {
        case F3_SB: insn.op = Op::SB; break;
        case F3_SH: insn.op = Op::SH; break;
        case F3_SW: insn.op = Op::SW; break;
        }
        break;

    case OPC_OP_IMM:
        insn.imm = imm_i(raw);
        insn.rs2 = 0;
        switch (f3) {
        case F3_ADD_SUB: insn.op = Op::ADDI;  break;
        case F3_SLT:     insn.op = Op::SLTI;  break;
        case F3_SLTU:    insn.op = Op::SLTIU; break;
        case F3_XOR:     insn.op = Op::XORI;  break;
        case F3_OR:      insn.op = Op::ORI;   break;
        case F3_AND:     insn.op = Op::ANDI;  break;
        // Shifts: imm[11:5] must be 0 (SLLI/SRLI) or 0b0100000 (SRAI)
        case F3_SLL:
            if (f7 == F7_BASE) { insn.op = Op::SLLI; insn.imm = imm_i(raw) & 0x1f; }
            break;
        case F3_SRL_SRA:
            if      (f7 == F7_BASE) { insn.op = Op::SRLI; insn.imm = imm_i(raw) & 0x1f; }
            else if (f7 == F7_ALT)  { insn.op = Op::SRAI; insn.imm = imm_i(raw) & 0x1f; }
            break;
        }
        break;

    case OPC_OP:
        insn.imm = 0;
        switch (f3) {
        case F3_ADD_SUB:
            if      (f7 == F7_BASE) insn.op = Op::ADD;
            else if (f7 == F7_ALT)  insn.op = Op::SUB;
            break;
        case F3_SLL:  if (f7 == F7_BASE) insn.op = Op::SLL;  break;
        case F3_SLT:  if (f7 == F7_BASE) insn.op = Op::SLT;  break;
        case F3_SLTU: if (f7 == F7_BASE) insn.op = Op::SLTU; break;
        case F3_XOR:  if (f7 == F7_BASE) insn.op = Op::XOR;  break;
        case F3_SRL_SRA:
            if      (f7 == F7_BASE) insn.op = Op::SRL;
            else if (f7 == F7_ALT)  insn.op = Op::SRA;
            break;
        case F3_OR:   if (f7 == F7_BASE) insn.op = Op::OR;   break;
        case F3_AND:  if (f7 == F7_BASE) insn.op = Op::AND;  break;
        }
        break;

    case OPC_MISC_MEM:
        insn.op  = Op::FENCE;
        insn.rd  = 0; insn.rs1 = 0; insn.rs2 = 0; insn.imm = 0;
        break;

    case OPC_SYSTEM:
        insn.rd  = 0; insn.rs1 = 0; insn.rs2 = 0; insn.imm = 0;
        switch (funct12(raw)) {
        case F12_ECALL:  insn.op = Op::ECALL;  break;
        case F12_EBREAK: insn.op = Op::EBREAK; break;
        }
        break;
    }

    return insn;
}

const char* op_name(Op op) {
    switch (op) {
    case Op::ADD:   return "add";   case Op::SUB:   return "sub";
    case Op::SLL:   return "sll";   case Op::SRL:   return "srl";
    case Op::SRA:   return "sra";   case Op::SLT:   return "slt";
    case Op::SLTU:  return "sltu";  case Op::AND:   return "and";
    case Op::OR:    return "or";    case Op::XOR:   return "xor";
    case Op::ADDI:  return "addi";  case Op::SLLI:  return "slli";
    case Op::SRLI:  return "srli";  case Op::SRAI:  return "srai";
    case Op::SLTI:  return "slti";  case Op::SLTIU: return "sltiu";
    case Op::ANDI:  return "andi";  case Op::ORI:   return "ori";
    case Op::XORI:  return "xori";
    case Op::LB:    return "lb";    case Op::LH:    return "lh";
    case Op::LW:    return "lw";    case Op::LBU:   return "lbu";
    case Op::LHU:   return "lhu";
    case Op::SB:    return "sb";    case Op::SH:    return "sh";
    case Op::SW:    return "sw";
    case Op::BEQ:   return "beq";   case Op::BNE:   return "bne";
    case Op::BLT:   return "blt";   case Op::BGE:   return "bge";
    case Op::BLTU:  return "bltu";  case Op::BGEU:  return "bgeu";
    case Op::JAL:   return "jal";   case Op::JALR:  return "jalr";
    case Op::LUI:   return "lui";   case Op::AUIPC: return "auipc";
    case Op::ECALL: return "ecall"; case Op::EBREAK:return "ebreak";
    case Op::FENCE: return "fence";
    case Op::ILLEGAL: return "ILLEGAL";
    }
    return "?";
}

}  // namespace cpusim::rv32i
