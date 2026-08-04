#include "cpusim/uarch/pipeline/mem_access.h"
#include "cpusim/isa/rv32i/defs.h"

namespace cpusim {

using namespace rv32i;

MemAccessStage::MemAccessStage(IMemory&                      dmem,
                               const Latch<pipeline::ExMem>& in,
                               Latch<pipeline::MemWb>&       out)
    : dmem_(dmem), in_(in), out_(out) {}

static bool is_store(Op op) {
    switch (op) {
        case Op::SB: case Op::SH: case Op::SW: return true;
        default:                               return false;
    }
}

static bool is_mem_op(Op op) {
    switch (op) {
        case Op::LB:  case Op::LH:  case Op::LW:
        case Op::LBU: case Op::LHU:
            return true;
        default:
            return is_store(op);
    }
}

void MemAccessStage::evaluate() {
    const pipeline::ExMem& ex = in_.read();
    stalling_ = false;

    if (!ex.valid) {
        out_.write(pipeline::MemWb{});  // bubble
        return;
    }

    // Ask the memory unit whether this access is served yet. While it
    // is not, stall and emit a bubble; the access runs once, below, on
    // the served cycle — so data is never affected by timing.
    if (is_mem_op(ex.op) && !dmem_.ready(ex.alu_out, is_store(ex.op))) {
        stalling_ = true;
        out_.write(pipeline::MemWb{});  // bubble while waiting
        return;
    }

    pipeline::MemWb wb;
    wb.pc    = ex.pc;
    wb.raw   = ex.raw;
    wb.op    = ex.op;
    wb.rd    = ex.rd;
    wb.valid = true;

    uint32_t addr = ex.alu_out;  // effective address for mem ops

    switch (ex.op) {
        // ── Loads ──────────────────────────────────────────────
        case Op::LW:
            wb.wb_val  = dmem_.load_word(addr);
            wb.mem_addr = addr;
            break;
        case Op::LH:
            wb.wb_val = static_cast<uint32_t>(
                static_cast<int32_t>(
                    static_cast<int16_t>(dmem_.load_half(addr))));
            wb.mem_addr = addr;
            break;
        case Op::LHU:
            wb.wb_val   = dmem_.load_half(addr);
            wb.mem_addr = addr;
            break;
        case Op::LB:
            wb.wb_val = static_cast<uint32_t>(
                static_cast<int32_t>(
                    static_cast<int8_t>(dmem_.load_byte(addr))));
            wb.mem_addr = addr;
            break;
        case Op::LBU:
            wb.wb_val   = dmem_.load_byte(addr);
            wb.mem_addr = addr;
            break;

        // ── Stores — record addr/val for tracer; no reg write ──
        case Op::SW: {
            uint32_t val = ex.rs2_val;
            dmem_.store_word(addr, val);
            wb.is_store = true; wb.mem_addr = addr; wb.mem_val = val;
            break;
        }
        case Op::SH: {
            uint32_t val = ex.rs2_val & 0xFFFFu;
            dmem_.store_half(addr, static_cast<uint16_t>(val));
            wb.is_store = true; wb.mem_addr = addr; wb.mem_val = val;
            break;
        }
        case Op::SB: {
            uint32_t val = ex.rs2_val & 0xFFu;
            dmem_.store_byte(addr, static_cast<uint8_t>(val));
            wb.is_store = true; wb.mem_addr = addr; wb.mem_val = val;
            break;
        }

        // ── All other ops — pass ALU result through ────────────
        default:
            wb.wb_val = ex.alu_out;
            break;
    }

    out_.write(wb);
}

void MemAccessStage::latch() {
    dmem_.tick();   // advance the memory unit's timing one cycle
    out_.latch();   // commit: bubble while stalling, result when served
}

}  // namespace cpusim
