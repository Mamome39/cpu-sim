#include "cpusim/uarch/pipeline/writeback.h"

namespace cpusim {

using namespace rv32i;

WritebackStage::WritebackStage(RegFile&                      rf,
                               const Latch<pipeline::MemWb>& in)
    : rf_(rf), in_(in) {}

bool WritebackStage::has_writeback(Op op) {
    switch (op) {
        case Op::SB: case Op::SH: case Op::SW:
        case Op::BEQ: case Op::BNE:
        case Op::BLT: case Op::BGE:
        case Op::BLTU: case Op::BGEU:
        case Op::ECALL: case Op::EBREAK:
        case Op::FENCE: case Op::ILLEGAL:
            return false;
        default:
            return true;
    }
}

void WritebackStage::evaluate() {
    const pipeline::MemWb& wb = in_.read();

    if (!wb.valid || !has_writeback(wb.op)) {
        pending_ = false;
        rd_      = 0;
        val_     = 0;
        return;
    }

    pending_ = true;
    rd_      = wb.rd;
    val_     = wb.wb_val;
}

void WritebackStage::latch() {
    if (pending_) {
        rf_.write(rd_, val_);
    }
    pending_ = false;
}

}  // namespace cpusim
