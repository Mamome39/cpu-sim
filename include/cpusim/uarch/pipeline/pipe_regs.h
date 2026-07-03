#pragma once
#include <cstdint>
#include "cpusim/isa/rv32i/decoder.h"

// Data structs carried in each pipeline latch.
// Default-constructed value is the bubble (NOP) state.

namespace cpusim::pipeline {

// IF/ID latch — raw fetch result
struct IfId {
    uint32_t pc  = 0;
    uint32_t raw = 0;  // NOP: addi x0, x0, 0
    bool     valid = false;
};

// ID/EX latch — decoded instruction + register values
struct IdEx {
    uint32_t      pc   = 0;
    uint32_t      raw  = 0;
    rv32i::Op     op   = rv32i::Op::ILLEGAL;
    uint8_t       rd   = 0;
    uint8_t       rs1  = 0;
    uint8_t       rs2  = 0;
    int32_t       imm  = 0;
    uint32_t      rs1_val = 0;
    uint32_t      rs2_val = 0;
    bool          valid   = false;
};

// EX/MEM latch — ALU result + store data
struct ExMem {
    uint32_t  pc        = 0;
    uint32_t  raw       = 0;
    rv32i::Op op        = rv32i::Op::ILLEGAL;
    uint8_t   rd        = 0;
    uint32_t  alu_out   = 0;  // result or effective address
    uint32_t  rs2_val   = 0;  // store data
    bool      branch_taken = false;
    uint32_t  branch_target = 0;
    bool      valid     = false;
};

// MEM/WB latch — value ready to write back
struct MemWb {
    uint32_t  pc       = 0;
    uint32_t  raw      = 0;
    rv32i::Op op       = rv32i::Op::ILLEGAL;
    uint8_t   rd       = 0;
    uint32_t  wb_val   = 0;    // ALU result or loaded data
    uint32_t  mem_addr = 0;    // effective address (stores only)
    uint32_t  mem_val  = 0;    // value written to memory (stores only)
    bool      is_store = false;
    bool      valid    = false;
};

}  // namespace cpusim::pipeline
