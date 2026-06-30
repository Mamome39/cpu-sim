#include "cpusim/uarch/pipeline/forward.h"

namespace cpusim {

ForwardUnit::ForwardUnit(const Latch<pipeline::ExMem>& ex_mem,
                         const Latch<pipeline::MemWb>& mem_wb)
    : ex_mem_(ex_mem), mem_wb_(mem_wb) {}

void ForwardUnit::evaluate() {
    prev_mem_wb_next_ = mem_wb_.read();
}

void ForwardUnit::latch() {
    prev_mem_wb_ = prev_mem_wb_next_;
}

uint32_t ForwardUnit::resolve(uint8_t reg, uint32_t cur) const {
    if (reg == 0) return 0;
    const pipeline::ExMem& ex = ex_mem_.read();
    const pipeline::MemWb& wb = mem_wb_.read();
    if (ex.valid && ex.rd == reg) return ex.alu_out;
    if (wb.valid && wb.rd == reg) return wb.wb_val;
    if (prev_mem_wb_.valid && prev_mem_wb_.rd == reg)
        return prev_mem_wb_.wb_val;
    return cur;
}

}  // namespace cpusim
