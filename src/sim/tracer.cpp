#include "cpusim/sim/tracer.h"
#include "cpusim/regfile.h"
#include <cstdio>
#include <cstring>

namespace cpusim {

void Tracer::record(const pipeline::MemWb& wb) {
    if (!wb.valid) return;

    char buf[96];
    int n = std::snprintf(buf, sizeof(buf),
        "core   0: 3 0x%08x (0x%08x)", wb.pc, wb.raw);

    if (wb.is_store) {
        n += std::snprintf(buf + n, sizeof(buf) - n,
            " mem 0x%08x 0x%08x", wb.mem_addr, wb.mem_val);
    } else if (wb.rd != 0) {
        n += std::snprintf(buf + n, sizeof(buf) - n,
            " %-4s 0x%08x", abi_name(wb.rd), wb.wb_val);
    }

    out_ << buf << '\n';
}

}  // namespace cpusim
