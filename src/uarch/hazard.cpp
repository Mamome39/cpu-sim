#include "cpusim/uarch/hazard.h"

namespace cpusim {

HazardUnit::HazardUnit(const Latch<pipeline::IfId>&  if_id,
                       const Latch<pipeline::IdEx>&  id_ex,
                       FetchStage&                   fetch,
                       DecodeStage&                  decode)
    : if_id_(if_id), id_ex_(id_ex), fetch_(fetch), decode_(decode) {}

bool HazardUnit::is_load(rv32i::Op op) {
    return op == rv32i::Op::LB  || op == rv32i::Op::LH  ||
           op == rv32i::Op::LW  || op == rv32i::Op::LBU ||
           op == rv32i::Op::LHU;
}

void HazardUnit::evaluate() {
    stall_ = false;

    const pipeline::IdEx& ex = id_ex_.read();
    const pipeline::IfId& fe = if_id_.read();

    if (!ex.valid)                      return;
    if (!is_load(ex.op) || ex.rd == 0) return;

    // Extract rs1 (bits 19:15) and rs2 (bits 24:20) from the raw
    // instruction currently in ID. Checked conservatively: if either
    // raw field matches the load's rd, we stall.
    uint8_t fe_rs1 = static_cast<uint8_t>((fe.raw >> 15) & 0x1Fu);
    uint8_t fe_rs2 = static_cast<uint8_t>((fe.raw >> 20) & 0x1Fu);

    if (ex.rd == fe_rs1 || ex.rd == fe_rs2) stall_ = true;
}

void HazardUnit::latch() {
    if (stall_) {
        fetch_.set_stall(true);
        decode_.set_flush(true);
    }
    stall_ = false;
}

}  // namespace cpusim
