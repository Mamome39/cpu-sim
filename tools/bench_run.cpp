#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include "cpusim/sim/core.h"
#include "cpusim/sim/tracer.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr,
            "usage: bench_run [--trace=<file>] <elf> [max_cycles]\n");
        return 1;
    }

    const char* trace_path = nullptr;
    const char* elf_path   = nullptr;
    uint64_t    max_cycles = ~0ULL;

    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--trace=", 8) == 0) {
            trace_path = argv[i] + 8;
        } else if (elf_path == nullptr) {
            elf_path = argv[i];
        } else {
            max_cycles = std::strtoull(argv[i], nullptr, 10);
        }
    }

    if (!elf_path) {
        std::fprintf(stderr, "error: no ELF path given\n");
        return 1;
    }

    constexpr uint32_t base     = 0x80000000u;
    constexpr size_t   mem_size = 0x10000u;    // 64 KiB

    try {
        cpusim::Core core(base, mem_size);
        core.load_elf(elf_path);

        std::ofstream                   trace_out;
        std::unique_ptr<cpusim::Tracer> tracer;
        if (trace_path) {
            trace_out.open(trace_path);
            if (!trace_out)
                std::fprintf(stderr,
                    "warn: cannot open trace file '%s'\n", trace_path);
            else {
                tracer = std::make_unique<cpusim::Tracer>(trace_out);
                core.set_tracer(tracer.get());
            }
        }

        auto t0 = std::chrono::steady_clock::now();
        core.run(max_cycles);
        auto t1 = std::chrono::steady_clock::now();

        double elapsed_s =
            std::chrono::duration<double>(t1 - t0).count();
        double mcps =
            static_cast<double>(core.cycles()) / elapsed_s / 1e6;

        std::printf("elf:       %s\n",  elf_path);
        std::printf("halted:    %s\n",
                    core.halted() ? "yes" : "no (cycle limit)");
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
