#pragma once
#include <cstdint>

// RV32I opcodes, funct3, and funct7 constants.
// All values are the raw bit patterns from the spec (no shifting).

namespace cpusim::rv32i {

// ── Opcodes [6:0] ─────────────────────────────────────────────────────────────
constexpr uint32_t OPC_LUI      = 0b0110111;
constexpr uint32_t OPC_AUIPC    = 0b0010111;
constexpr uint32_t OPC_JAL      = 0b1101111;
constexpr uint32_t OPC_JALR     = 0b1100111;
constexpr uint32_t OPC_BRANCH   = 0b1100011;
constexpr uint32_t OPC_LOAD     = 0b0000011;
constexpr uint32_t OPC_STORE    = 0b0100011;
constexpr uint32_t OPC_OP_IMM   = 0b0010011;
constexpr uint32_t OPC_OP       = 0b0110011;
constexpr uint32_t OPC_MISC_MEM = 0b0001111;  // FENCE
constexpr uint32_t OPC_SYSTEM   = 0b1110011;  // ECALL / EBREAK

// ── funct3 — BRANCH ───────────────────────────────────────────────────────────
constexpr uint32_t F3_BEQ  = 0b000;
constexpr uint32_t F3_BNE  = 0b001;
constexpr uint32_t F3_BLT  = 0b100;
constexpr uint32_t F3_BGE  = 0b101;
constexpr uint32_t F3_BLTU = 0b110;
constexpr uint32_t F3_BGEU = 0b111;

// ── funct3 — LOAD ─────────────────────────────────────────────────────────────
constexpr uint32_t F3_LB  = 0b000;
constexpr uint32_t F3_LH  = 0b001;
constexpr uint32_t F3_LW  = 0b010;
constexpr uint32_t F3_LBU = 0b100;
constexpr uint32_t F3_LHU = 0b101;

// ── funct3 — STORE ────────────────────────────────────────────────────────────
constexpr uint32_t F3_SB = 0b000;
constexpr uint32_t F3_SH = 0b001;
constexpr uint32_t F3_SW = 0b010;

// ── funct3 — OP / OP_IMM ─────────────────────────────────────────────────────
constexpr uint32_t F3_ADD_SUB = 0b000;  // ADD/SUB (OP), ADDI (OP_IMM)
constexpr uint32_t F3_SLL     = 0b001;
constexpr uint32_t F3_SLT     = 0b010;
constexpr uint32_t F3_SLTU    = 0b011;
constexpr uint32_t F3_XOR     = 0b100;
constexpr uint32_t F3_SRL_SRA = 0b101;  // distinguished by funct7
constexpr uint32_t F3_OR      = 0b110;
constexpr uint32_t F3_AND     = 0b111;

// ── funct7 ────────────────────────────────────────────────────────────────────
constexpr uint32_t F7_BASE = 0b0000000;  // ADD, SRL, SLLI, SRLI
constexpr uint32_t F7_ALT  = 0b0100000;  // SUB, SRA, SRAI

// ── funct12 — SYSTEM (bits [31:20] of I-type) ────────────────────────────────
constexpr uint32_t F12_ECALL  = 0b000000000000;
constexpr uint32_t F12_EBREAK = 0b000000000001;

}  // namespace cpusim::rv32i
