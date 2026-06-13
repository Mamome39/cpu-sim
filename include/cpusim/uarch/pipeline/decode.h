#pragma once
#include <cstdint>
#include "cpusim/regfile.h"
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"

namespace cpusim {

// ID stage — decodes instruction and reads register file.
//
// evaluate(): reads IfId latch, decodes raw word, reads rs1/rs2
//             from the register file, writes IdEx shadow.
//             Propagates a bubble when upstream is invalid.
//
// latch():    flush → bubble inserted into IdEx latch.
//             stall → skip; IdEx holds its current value.
//             normal → commit shadow to IdEx register.
//
// Flush is triggered by the EX stage on a branch redirect.
// Stall is driven by the hazard unit (load-use hazard).

class DecodeStage : public Stage {
public:
    DecodeStage(RegFile&              rf,
                const Latch<pipeline::IfId>&  in,
                Latch<pipeline::IdEx>&        out);

    void evaluate() override;
    void latch()    override;

    void set_stall(bool s) { stall_ = s; }
    void set_flush(bool f) { flush_ = f; }

private:
    RegFile&                      rf_;
    const Latch<pipeline::IfId>&  in_;
    Latch<pipeline::IdEx>&        out_;

    bool stall_ = false;
    bool flush_ = false;
};

}  // namespace cpusim
