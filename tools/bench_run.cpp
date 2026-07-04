#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "cpusim/sim/core.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: bench_run <elf> [max_cycles]\n");
        return 1;
    }

    const char*    path       = argv[1];
    uint64_t       max_cycles = (argc >= 3)
                                ? std::strtoull(argv[2], nullptr, 10)
                                : ~0ULL;
    uint32_t       base       = 0x80000000u;
    size_t         mem_size   = 0x10000u;    // 64 KiB

    try {
        cpusim::Core core(base, mem_size);
        core.load_elf(path);

        auto t0 = std::chrono::steady_clock::now();
        core.run(max_cycles);
        auto t1 = std::chrono::steady_clock::now();

        double elapsed_s =
            std::chrono::duration<double>(t1 - t0).count();
        double mcps = static_cast<double>(core.cycles()) / elapsed_s / 1e6;

        std::printf("elf:       %s\n",  path);
        std::printf("halted:    %s\n",  core.halted() ? "yes" : "no (cycle limit)");
        std::printf("cycles:    %llu\n",
                    static_cast<unsigned long long>(core.cycles()));
        std::printf("wall time: %.4f s\n",  elapsed_s);
        std::printf("sim speed: %.2f Mcycles/s\n", mcps);
        std::printf("\n");
        core.read_regs();

        return core.halted() ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }
}
