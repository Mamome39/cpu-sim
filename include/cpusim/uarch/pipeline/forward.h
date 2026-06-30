#pragma once
#include <cstdint>
#include "cpusim/uarch/latch.h"
#include "cpusim/uarch/stage.h"
#include "cpusim/uarch/pipeline/pipe_regs.h"

namespace cpusim {

// ForwardUnit — resolves data hazards via operand forwarding.
//
// Three forwarding paths (highest priority first):
//   EX->EX   ex_mem.alu_out  1-instruction gap
//   MEM->EX  mem_wb.wb_val   2-instruction gap
//   WB->EX   prev_mem_wb     3-instruction gap
//
// evaluate(): snapshots mem_wb into prev_mem_wb_next_. Must run
//             before forward.latch() and mem.latch() in the same
//             clock cycle.
//
// latch():    commits the snapshot to prev_mem_wb_. Must run before
//             mem.latch() overwrites mem_wb.
//
// resolve():  returns the forwarded value for a given register.
//             Reads only committed latch state — safe to call at
//             any point during evaluate().

class ForwardUnit : public Stage {
public:
    ForwardUnit(const Latch<pipeline::ExMem>& ex_mem,
                const Latch<pipeline::MemWb>& mem_wb);

    void evaluate() override;
    void latch()    override;

    uint32_t resolve(uint8_t reg, uint32_t cur) const;

private:
    const Latch<pipeline::ExMem>& ex_mem_;
    const Latch<pipeline::MemWb>& mem_wb_;

    pipeline::MemWb prev_mem_wb_      {};
    pipeline::MemWb prev_mem_wb_next_ {};
};

}  // namespace cpusim
