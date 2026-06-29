#include "cpusim/uarch/pipeline/mem_access.h"
#include "cpusim/isa/rv32i/defs.h"

namespace cpusim {

using namespace rv32i;

MemAccessStage::MemAccessStage(IMemory&                      dmem,
                               const Latch<pipeline::ExMem>& in,
                               Latch<pipeline::MemWb>&       out)
    : dmem_(dmem), in_(in), out_(out) {}

void MemAccessStage::evaluate() {
    const pipeline::ExMem& ex = in_.read();

    if (!ex.valid) {
        out_.write(pipeline::MemWb{});  // bubble
        return;
    }

    pipeline::MemWb wb;
    wb.pc    = ex.pc;
    wb.op    = ex.op;
    wb.rd    = ex.rd;
    wb.valid = true;

    uint32_t addr = ex.alu_out;  // effective address for mem ops

    switch (ex.op) {
        // ── Loads ──────────────────────────────────────────────
        case Op::LW:
            wb.wb_val = dmem_.load_word(addr);
            break;
        case Op::LH:
            wb.wb_val = static_cast<uint32_t>(
                static_cast<int32_t>(
                    static_cast<int16_t>(dmem_.load_half(addr))));
            break;
        case Op::LHU:
            wb.wb_val = dmem_.load_half(addr);
            break;
        case Op::LB:
            wb.wb_val = static_cast<uint32_t>(
                static_cast<int32_t>(
                    static_cast<int8_t>(dmem_.load_byte(addr))));
            break;
        case Op::LBU:
            wb.wb_val = dmem_.load_byte(addr);
            break;

        // ── Stores — write to memory, wb_val unused by WB ─────
        case Op::SW:
            dmem_.store_word(addr, ex.rs2_val);
            wb.wb_val = 0;
            break;
        case Op::SH:
            dmem_.store_half(addr,
                static_cast<uint16_t>(ex.rs2_val & 0xFFFF));
            wb.wb_val = 0;
            break;
        case Op::SB:
            dmem_.store_byte(addr,
                static_cast<uint8_t>(ex.rs2_val & 0xFF));
            wb.wb_val = 0;
            break;

        // ── All other ops — pass ALU result through ────────────
        default:
            wb.wb_val = ex.alu_out;
            break;
    }

    out_.write(wb);
}

void MemAccessStage::latch() {
    out_.latch();
}

}  // namespace cpusim
