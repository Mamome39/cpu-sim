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
            "usage: bench_run [--trace=<file>] [--cycles=<file>] "
            "[--mem-latency=<n>] [--dcache] [--dcache-ways=<n>] "
            "[--imem-latency=<n>] [--icache] [--icache-ways=<n>] "
            "[--iprefetch] [--iprefetch-degree=<n>] "
            "[--bpred] <elf> [max_cycles]\n");
        return 1;
    }

    const char* trace_path  = nullptr;  // Spike-format commit log
    const char* cycles_path = nullptr;  // cycle-by-cycle view
    const char* elf_path    = nullptr;
    uint64_t    max_cycles  = ~0ULL;
    unsigned    mem_latency  = 5;         // data-memory serve latency
    bool        dcache       = false;     // enable the L1 D-cache
    unsigned    dcache_ways  = 1;         // D-cache associativity
    unsigned    imem_latency = 1;         // instruction-memory serve latency
    bool        icache       = false;     // enable the L1 I-cache
    unsigned    icache_ways  = 1;         // I-cache associativity
    bool        iprefetch        = false; // enable I-cache prefetch
    unsigned    iprefetch_degree = 1;     // prefetch chain length
    bool        bpred        = false;     // enable the branch predictor

    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--trace=", 8) == 0) {
            trace_path = argv[i] + 8;
        } else if (std::strncmp(argv[i], "--cycles=", 9) == 0) {
            cycles_path = argv[i] + 9;
        } else if (std::strncmp(argv[i], "--mem-latency=", 14) == 0) {
            mem_latency = static_cast<unsigned>(
                std::strtoul(argv[i] + 14, nullptr, 10));
        } else if (std::strcmp(argv[i], "--dcache") == 0) {
            dcache = true;
        } else if (std::strncmp(argv[i], "--dcache-ways=", 14) == 0) {
            dcache_ways = static_cast<unsigned>(
                std::strtoul(argv[i] + 14, nullptr, 10));
        } else if (std::strncmp(argv[i], "--imem-latency=", 15) == 0) {
            imem_latency = static_cast<unsigned>(
                std::strtoul(argv[i] + 15, nullptr, 10));
        } else if (std::strcmp(argv[i], "--icache") == 0) {
            icache = true;
        } else if (std::strncmp(argv[i], "--icache-ways=", 14) == 0) {
            icache_ways = static_cast<unsigned>(
                std::strtoul(argv[i] + 14, nullptr, 10));
        } else if (std::strcmp(argv[i], "--iprefetch") == 0) {
            iprefetch = true;
        } else if (std::strncmp(argv[i], "--iprefetch-degree=", 19) == 0) {
            iprefetch_degree = static_cast<unsigned>(
                std::strtoul(argv[i] + 19, nullptr, 10));
        } else if (std::strcmp(argv[i], "--bpred") == 0) {
            bpred = true;
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

    cpusim::SimConfig cfg;
    cfg.dmem_latency_cycles = mem_latency;   // becomes the miss penalty
    cfg.dcache_enabled      = dcache;
    cfg.dcache_ways         = dcache_ways;
    cfg.imem_latency_cycles = imem_latency;  // becomes the I-miss penalty
    cfg.icache_enabled      = icache;
    cfg.icache_ways         = icache_ways;
    cfg.icache_prefetch_enabled = iprefetch;
    cfg.icache_prefetch_degree  = iprefetch_degree;
    cfg.bpred_enabled       = bpred;

    try {
        cpusim::Core core(cfg);
        core.load_elf(elf_path);

        // One tracer at a time; --cycles takes priority if both given.
        using Fmt = cpusim::Tracer::Format;
        const char* out_path = cycles_path ? cycles_path : trace_path;
        Fmt         out_fmt  = cycles_path ? Fmt::Cycle  : Fmt::Commit;

        std::ofstream                   trace_out;
        std::unique_ptr<cpusim::Tracer> tracer;
        if (out_path) {
            trace_out.open(out_path);
            if (!trace_out)
                std::fprintf(stderr,
                    "warn: cannot open trace file '%s'\n", out_path);
            else {
                tracer = std::make_unique<cpusim::Tracer>(
                    trace_out, out_fmt);
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
        std::printf("mem lat:   %u cycle(s)%s\n", mem_latency,
                    dcache ? " (miss penalty)" : "");
        std::printf("dcache:    %s", dcache ? "on" : "off");
        if (dcache) std::printf(" (%u-way)", dcache_ways);
        std::printf("\n");
        std::printf("imem lat:  %u cycle(s)%s\n", imem_latency,
                    icache ? " (miss penalty)" : "");
        std::printf("icache:    %s", icache ? "on" : "off");
        if (icache) std::printf(" (%u-way)", icache_ways);
        std::printf("\n");
        std::printf("iprefetch: %s", iprefetch ? "on" : "off");
        if (iprefetch) std::printf(" (degree %u)", iprefetch_degree);
        std::printf("\n");
        std::printf("bpred:     %s\n", bpred ? "on" : "off");
        const cpusim::SimStats& s = core.stats();
        unsigned long long correct = s.branch_correct;
        unsigned long long mispred = s.branch_mispredicts;
        unsigned long long total   = correct + mispred;
        std::printf("branches:  %llu\n", total);
        std::printf("correct:   %llu\n", correct);
        std::printf("mispred:   %llu\n", mispred);
        if (iprefetch)
            std::printf("pf useful/useless: %llu/%llu\n",
                        static_cast<unsigned long long>(
                            s.icache_prefetch_useful),
                        static_cast<unsigned long long>(
                            s.icache_prefetch_useless));
        if (total)
            std::printf("accuracy:  %.2f%%\n",
                        100.0 * static_cast<double>(correct) / total);
        std::printf("halted:    %s\n",
                    core.halted() ? "yes" : "no (cycle limit)");
        std::printf("cycles:    %llu\n",
                    static_cast<unsigned long long>(s.cycles));
        std::printf("insts:     %llu\n",
                    static_cast<unsigned long long>(s.instructions));
        if (s.cycles)
            std::printf("IPC:       %.3f\n",
                        static_cast<double>(s.instructions) /
                        static_cast<double>(s.cycles));
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
