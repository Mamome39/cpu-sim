#include "cpusim/sim/tracer.h"
#include "cpusim/isa/rv32i/defs.h"
#include "cpusim/isa/rv32i/decoder.h"
#include "cpusim/regfile.h"
#include <cstdio>

namespace cpusim {

using rv32i::Op;

static bool is_load_op(Op op) {
    return op == Op::LW  || op == Op::LH  || op == Op::LHU ||
           op == Op::LB  || op == Op::LBU;
}

// Spike prints a store's value at its access width: SB → 2 hex
// digits, SH → 4, SW → 8. Return the hex digit count for op.
static int store_hex_digits(Op op) {
    switch (op) {
        case Op::SB: return 2;
        case Op::SH: return 4;
        default:     return 8;  // SW
    }
}

// Format the architectural side effect (leading space included):
//   " x<rd>  0x<val> [mem 0x<addr>]"  reg write / load
//   " mem 0x<addr> 0x<val>"           store, value at access width
//   ""                                no side effect
// Shared by both trace formats so effect columns stay identical.
static int format_effect(char* buf, size_t size,
                         const pipeline::MemWb& wb) {
    if (wb.is_store) {
        int w = store_hex_digits(wb.op);
        return std::snprintf(buf, size, " mem 0x%08x 0x%0*x",
                             wb.mem_addr, w, wb.mem_val);
    }
    if (wb.rd != 0) {
        int n = std::snprintf(buf, size, " x%-2d 0x%08x",
                              wb.rd, wb.wb_val);
        if (is_load_op(wb.op))
            n += std::snprintf(buf + n, size - n,
                               " mem 0x%08x", wb.mem_addr);
        return n;
    }
    buf[0] = '\0';
    return 0;
}

// Disassemble a raw word into "mnem  operands" (Cycle format only).
// Registers use ABI names; branch/jump targets are absolute (pc+imm).
static void disasm(char* buf, size_t size, uint32_t raw, uint32_t pc) {
    rv32i::Instruction in = rv32i::decode(raw, pc);
    const char* m   = rv32i::op_name(in.op);
    const char* rd  = abi_name(in.rd);
    const char* rs1 = abi_name(in.rs1);
    const char* rs2 = abi_name(in.rs2);

    switch (in.op) {
        case Op::ADD:  case Op::SUB:  case Op::SLL: case Op::SRL:
        case Op::SRA:  case Op::SLT:  case Op::SLTU: case Op::AND:
        case Op::OR:   case Op::XOR:
            std::snprintf(buf, size, "%-5s %s, %s, %s", m, rd, rs1, rs2);
            break;
        case Op::ADDI: case Op::SLTI: case Op::SLTIU:
        case Op::ANDI: case Op::ORI:  case Op::XORI:
            std::snprintf(buf, size, "%-5s %s, %s, %d", m, rd, rs1, in.imm);
            break;
        case Op::SLLI: case Op::SRLI: case Op::SRAI:
            std::snprintf(buf, size, "%-5s %s, %s, %d",
                          m, rd, rs1, in.imm & 0x1f);
            break;
        case Op::LB: case Op::LH: case Op::LW: case Op::LBU: case Op::LHU:
            std::snprintf(buf, size, "%-5s %s, %d(%s)", m, rd, in.imm, rs1);
            break;
        case Op::SB: case Op::SH: case Op::SW:
            std::snprintf(buf, size, "%-5s %s, %d(%s)", m, rs2, in.imm, rs1);
            break;
        case Op::BEQ: case Op::BNE: case Op::BLT:
        case Op::BGE: case Op::BLTU: case Op::BGEU:
            std::snprintf(buf, size, "%-5s %s, %s, 0x%08x",
                          m, rs1, rs2, pc + in.imm);
            break;
        case Op::JAL:
            std::snprintf(buf, size, "%-5s %s, 0x%08x", m, rd, pc + in.imm);
            break;
        case Op::JALR:
            std::snprintf(buf, size, "%-5s %s, %d(%s)", m, rd, in.imm, rs1);
            break;
        case Op::LUI: case Op::AUIPC:
            std::snprintf(buf, size, "%-5s %s, 0x%x",
                          m, rd, static_cast<uint32_t>(in.imm) >> 12);
            break;
        default:  // ECALL, EBREAK, FENCE, ILLEGAL — mnemonic only
            std::snprintf(buf, size, "%s", m);
            break;
    }
}

void Tracer::record(const pipeline::MemWb& wb, uint64_t cycle) {
    char eff[64];

    if (fmt_ == Format::Cycle) {
        // One line every cycle: cycle | pc  asm.
        // Stalls/bubbles show only the cycle number.
        if (!wb.valid) {
            char line[16];
            std::snprintf(line, sizeof(line), "%6llu |",
                          static_cast<unsigned long long>(cycle));
            out_ << line << '\n';
            return;
        }
        char dis[40];
        disasm(dis, sizeof(dis), wb.raw, wb.pc);
        char line[80];
        std::snprintf(line, sizeof(line), "%6llu | 0x%08x  %s",
            static_cast<unsigned long long>(cycle), wb.pc, dis);
        out_ << line << '\n';
        return;
    }

    // Commit format — Spike --log-commits; bubbles emit nothing.
    if (!wb.valid) return;
    format_effect(eff, sizeof(eff), wb);
    char line[128];
    std::snprintf(line, sizeof(line),
        "core   0: 3 0x%08x (0x%08x)%s", wb.pc, wb.raw, eff);
    out_ << line << '\n';
}

}  // namespace cpusim
