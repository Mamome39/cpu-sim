#include "cpusim/uarch/pipeline/decode.h"
#include "cpusim/isa/rv32i/decoder.h"

namespace cpusim {

DecodeStage::DecodeStage(RegFile&                      rf,
                         const Latch<pipeline::IfId>&  in,
                         Latch<pipeline::IdEx>&        out)
    : rf_(rf), in_(in), out_(out) {}

void DecodeStage::evaluate() {
    const pipeline::IfId& fetch = in_.read();

    if (!fetch.valid) {
        out_.write(pipeline::IdEx{});  // bubble
        return;
    }

    rv32i::Instruction insn = rv32i::decode(fetch.raw, fetch.pc);

    pipeline::IdEx ex;
    ex.pc      = fetch.pc;
    ex.raw     = fetch.raw;
    ex.op      = insn.op;
    ex.rd      = insn.rd;
    ex.rs1     = insn.rs1;
    ex.rs2     = insn.rs2;
    ex.imm     = insn.imm;
    ex.rs1_val = rf_.read(insn.rs1);  // combinational reg read
    ex.rs2_val = rf_.read(insn.rs2);  // same cycle as decode
    ex.valid   = true;

    out_.write(ex);
}

void DecodeStage::latch() {
    if (flush_) {
        out_.flush();
    } else if (!stall_) {
        out_.latch();
    }
    stall_ = false;
    flush_ = false;
}

}  // namespace cpusim
