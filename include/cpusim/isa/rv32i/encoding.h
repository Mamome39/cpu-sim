#pragma once
#include <cstdint>

// Bit-field extraction for the six RV32I instruction formats.
//
// RV32I encodes immediates by scattering bits across non-contiguous fields —
// each format has its own layout. All imm_* functions return a sign-extended
// int32_t matching the value the hardware would produce.
//
// Format summary (spec Figure 2.3):
//
//  R:  funct7[31:25] rs2[24:20] rs1[19:15] funct3[14:12] rd[11:7]  opcode[6:0]
//  I:  imm[31:20]              rs1[19:15] funct3[14:12] rd[11:7]  opcode[6:0]
//  S:  imm[31:25]   rs2[24:20] rs1[19:15] funct3[14:12] imm[11:7] opcode[6:0]
//  B:  imm[31:25]   rs2[24:20] rs1[19:15] funct3[14:12] imm[11:7] opcode[6:0]
//  U:  imm[31:12]                                        rd[11:7]  opcode[6:0]
//  J:  imm[31:12]                                        rd[11:7]  opcode[6:0]

namespace cpusim::rv32i::enc {

// ── Fixed fields (same position in every format) ─────────────────────────────
inline uint32_t opcode(uint32_t w) { return  w        & 0x7f; }
inline uint32_t rd    (uint32_t w) { return (w >>  7) & 0x1f; }
inline uint32_t funct3(uint32_t w) { return (w >> 12) & 0x07; }
inline uint32_t rs1   (uint32_t w) { return (w >> 15) & 0x1f; }
inline uint32_t rs2   (uint32_t w) { return (w >> 20) & 0x1f; }
inline uint32_t funct7(uint32_t w) { return (w >> 25) & 0x7f; }

// funct12 is the full upper 12 bits — used to distinguish ECALL / EBREAK
inline uint32_t funct12(uint32_t w) { return w >> 20; }

// ── Sign extension helper ─────────────────────────────────────────────────────
// Extends a value of 'bits' width to a signed 32-bit integer.
inline int32_t sext(uint32_t val, int bits) {
    uint32_t sign = 1u << (bits - 1);
    return static_cast<int32_t>((val ^ sign) - sign);
}

// ── Immediate extraction ──────────────────────────────────────────────────────

// I-type: imm[11:0] = w[31:20]
inline int32_t imm_i(uint32_t w) {
    return sext(w >> 20, 12);
}

// S-type: imm[11:5] = w[31:25], imm[4:0] = w[11:7]
inline int32_t imm_s(uint32_t w) {
    uint32_t imm = ((w >> 25) << 5) | ((w >> 7) & 0x1f);
    return sext(imm, 12);
}

// B-type: imm[12] = w[31], imm[11] = w[7], imm[10:5] = w[30:25], imm[4:1] = w[11:8]
// Note: imm[0] is always 0 — branches are 2-byte aligned.
inline int32_t imm_b(uint32_t w) {
    uint32_t imm = ((w >> 31)        << 12)
                 | (((w >>  7) & 0x1) << 11)
                 | (((w >> 25) & 0x3f) << 5)
                 | (((w >>  8) & 0xf)  << 1);
    return sext(imm, 13);
}

// U-type: imm[31:12] = w[31:12], lower 12 bits are zero.
inline int32_t imm_u(uint32_t w) {
    return static_cast<int32_t>(w & 0xfffff000u);
}

// J-type: imm[20] = w[31], imm[10:1] = w[30:21], imm[11] = w[20], imm[19:12] = w[19:12]
// Note: imm[0] is always 0 — jumps are 2-byte aligned.
inline int32_t imm_j(uint32_t w) {
    uint32_t imm = ((w >> 31)          << 20)
                 | (((w >> 12) & 0xff)  << 12)
                 | (((w >> 20) & 0x1)   << 11)
                 | (((w >> 21) & 0x3ff) <<  1);
    return sext(imm, 21);
}

}  // namespace cpusim::rv32i::enc
