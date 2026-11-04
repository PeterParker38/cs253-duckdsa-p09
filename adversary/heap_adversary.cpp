// heap_adversary.cpp -- Generates cancellation-heavy streams keeping physical storage high
// Exploits the purge condition (cancelled > live) by keeping cancelled <= live,
// causing heap physical storage to grow toward the total number of inserted events without purging.
#include <cstdio>
#include <vector>
#include <fstream>
#include <chrono>
#include "../include/rng.hpp"
#include "../src/structA/heap.hpp"

using namespace structA;

struct HeapAdversaryResult {
    const char* strategy;
    long long operations;
    long long inserted;
    long long cancelled;
    long long live;
    long long physical_size;
    std::uint64_t purges;
    double bloat_ratio; // physical_size / live
    double time_ms;
};

// Adversarial strategy:
// Insert 2 events, immediately cancel 1.
// At any moment: live = inserted - cancelled = k, cancelled = k.
// Since cancelled <= live (k <= k), cancelled > live is NEVER satisfied!
// Result: ZERO purges occur, and physical size = inserted = 2 * live.
static HeapAdversaryResult runAdversarialStream(long long n) {
    HeapEventSet heap;
    std::uint64_t seq = 0;
    std::vector<Event> can_cancel;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (long long i = 0; i < n; i++) {
        // Insert 2 events
        Event e1{100 + i, seq++, 0, 0};
        Event e2{100 + i, seq++, 0, 0};
        heap.insert(e1);
        heap.insert(e2);
        can_cancel.push_back(e1);

        // Cancel 1 event
        Event to_cancel = can_cancel.back();
        can_cancel.pop_back();
        heap.cancel(to_cancel);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    long long live = heap.size();
    long long phys = heap.physicalSize();
    long long cancelled = heap.cancelledCount();
    long long inserted = seq;

    return HeapAdversaryResult{
        "adversarial_purge_prevention",
        n,
        inserted,
        cancelled,
        live,
        phys,
        heap.purges(),
        live > 0 ? (double)phys / live : 1.0,
        ms
    };
}

// Benign / Random strategy for comparison:
// Events are inserted and cancelled randomly (e.g. 60% cancel rate).
// Purges trigger naturally, periodically reclaiming space.
static HeapAdversaryResult runBenignStream(long long n, std::uint64_t seed) {
    HeapEventSet heap;
    Rng rng(seed);
    std::uint64_t seq = 0;
    std::vector<Event> live_events;

    auto t0 = std::chrono::high_resolution_clock::now();
    for (long long i = 0; i < n * 2; i++) {
        if (live_events.empty() || rng.range(0, 99) < 40) {
            Event e{rng.range(1, 10000), seq++, 0, 0};
            heap.insert(e);
            live_events.push_back(e);
        } else {
            size_t idx = (size_t)rng.range(0, (long long)live_events.size() - 1);
            Event e = live_events[idx];
            live_events.erase(live_events.begin() + idx);
            heap.cancel(e);
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    long long live = heap.size();
    long long phys = heap.physicalSize();
    long long cancelled = heap.cancelledCount();
    long long inserted = seq;

    return HeapAdversaryResult{
        "benign_random",
        n,
        inserted,
        cancelled,
        live,
        phys,
        heap.purges(),
        live > 0 ? (double)phys / live : 1.0,
        ms
    };
}

int main() {
    std::printf("=== Adversarial Benchmark: Heap Storage Bloat Stress ===\n");
    std::ofstream csv("results/raw/heap_adversary.csv");
    csv << "strategy,target_ops,inserted,cancelled,live,physical_size,purges,bloat_ratio,time_ms\n";

    std::vector<long long> steps = {5000, 10000, 25000, 50000, 100000, 250000};
    for (long long n : steps) {
        HeapAdversaryResult adv = runAdversarialStream(n);
        HeapAdversaryResult ben = runBenignStream(n, 98765 + n);

        std::printf("N=%-6lld | Adv: ins=%-7lld live=%-6lld phys=%-7lld purges=%-3llu bloat=%.2fx (%.1f ms)\n",
                    n, adv.inserted, adv.live, adv.physical_size, (unsigned long long)adv.purges, adv.bloat_ratio, adv.time_ms);
        std::printf("       | Ben: ins=%-7lld live=%-6lld phys=%-7lld purges=%-3llu bloat=%.2fx (%.1f ms)\n",
                    ben.inserted, ben.live, ben.physical_size, (unsigned long long)ben.purges, ben.bloat_ratio, ben.time_ms);

        auto write_csv = [&](const HeapAdversaryResult& r) {
            csv << r.strategy << "," << r.operations << "," << r.inserted << ","
                << r.cancelled << "," << r.live << "," << r.physical_size << ","
                << r.purges << "," << r.bloat_ratio << "," << r.time_ms << "\n";
        };
        write_csv(adv);
        write_csv(ben);
    }

    csv.close();
    std::printf("Saved adversarial results to results/raw/heap_adversary.csv\n");
    return 0;
}
