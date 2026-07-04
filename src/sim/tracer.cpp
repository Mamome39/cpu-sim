#include "cpusim/sim/tracer.h"
#include "cpusim/isa/rv32i/defs.h"
#include <cstdio>

namespace cpusim {

using rv32i::Op;

static bool is_load_op(Op op) {
    return op == Op::LW  || op == Op::LH  || op == Op::LHU ||
           op == Op::LB  || op == Op::LBU;
}

void Tracer::record(const pipeline::MemWb& wb) {
    if (!wb.valid) return;

    char buf[96];
    int n = std::snprintf(buf, sizeof(buf),
        "core   0: 3 0x%08x (0x%08x)", wb.pc, wb.raw);

    if (wb.is_store) {
        // Spike: "mem 0x<addr> 0x<val>"
        n += std::snprintf(buf + n, sizeof(buf) - n,
            " mem 0x%08x 0x%08x", wb.mem_addr, wb.mem_val);
    } else if (wb.rd != 0) {
        // Spike: "x<N>  0x<val>" (field width 2 for the register number)
        n += std::snprintf(buf + n, sizeof(buf) - n,
            " x%-2d 0x%08x", wb.rd, wb.wb_val);
        if (is_load_op(wb.op))
            n += std::snprintf(buf + n, sizeof(buf) - n,
                " mem 0x%08x", wb.mem_addr);
    }

    out_ << buf << '\n';
}

}  // namespace cpusim
