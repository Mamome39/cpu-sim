#pragma once
#include "cpusim/regfile.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"

namespace cpusim {

// WB stage — writes results back to the register file.
//
// evaluate(): reads MemWb, decides whether a writeback is needed,
//             stores the pending write internally.
//             Stores, branches, and system ops do not write rd.
//
// latch():    commits the pending register write.
//             RegFile::write() already guards x0 — no special case
//             needed here beyond checking has_writeback().
//
// No output latch: WB is the last stage.

class WritebackStage : public Stage {
public:
    WritebackStage(RegFile& rf, const Latch<pipeline::MemWb>& in);

    void evaluate() override;
    void latch()    override;

    // True if the last evaluate() will write a register.
    // Used by the hazard unit to detect WB→ID forwarding opportunities.
    bool     pending_write() const { return pending_; }
    uint8_t  pending_rd()    const { return rd_; }
    uint32_t pending_val()   const { return val_; }

private:
    RegFile&                      rf_;
    const Latch<pipeline::MemWb>& in_;

    bool     pending_ = false;
    uint8_t  rd_      = 0;
    uint32_t val_     = 0;

    static bool has_writeback(rv32i::Op op);
};

}  // namespace cpusim
