// avl_adversary.cpp -- Generates adversarial insertion streams forcing frequent AVL rotations
// Measures rotation frequency on monotonic vs random streams, saving data to results/raw/avl_adversary.csv
#include <cstdio>
#include <vector>
#include <chrono>
#include <fstream>
#include "../include/rng.hpp"
#include "../src/structB/avl.hpp"

using namespace structB;

struct AdversaryRun {
    const char* pattern;
    int n;
    std::uint64_t left_rot;
    std::uint64_t right_rot;
    std::uint64_t total_rot;
    double rot_ratio; // rotations per insertion
    int final_height;
    double time_ms;
};

static AdversaryRun runMonotonic(int n) {
    AvlTree tree;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        tree.insert(Event{(long long)i, (std::uint64_t)i, 0, 0});
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return AdversaryRun{
        "monotonic_increasing",
        n,
        tree.leftRotations(),
        tree.rightRotations(),
        tree.totalRotations(),
        (double)tree.totalRotations() / n,
        tree.height(),
        ms
    };
}

static AdversaryRun runZigzag(int n) {
    AvlTree tree;
    auto t0 = std::chrono::high_resolution_clock::now();
    // Sawtooth pattern triggering double rotations (RL and LR)
    for (int i = 0; i < n; i += 2) {
        tree.insert(Event{(long long)(i + 1), (std::uint64_t)i, 0, 0});
        if (i + 1 < n) {
            tree.insert(Event{(long long)i, (std::uint64_t)(i + 1), 0, 0});
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return AdversaryRun{
        "sawtooth_zigzag",
        n,
        tree.leftRotations(),
        tree.rightRotations(),
        tree.totalRotations(),
        (double)tree.totalRotations() / n,
        tree.height(),
        ms
    };
}

static AdversaryRun runRandom(int n, std::uint64_t seed) {
    AvlTree tree;
    Rng rng(seed);
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        long long t = rng.range(0, 1000000000LL);
        tree.insert(Event{t, (std::uint64_t)i, 0, 0});
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return AdversaryRun{
        "uniform_random",
        n,
        tree.leftRotations(),
        tree.rightRotations(),
        tree.totalRotations(),
        (double)tree.totalRotations() / n,
        tree.height(),
        ms
    };
}

int main() {
    std::printf("=== Adversarial Benchmark: AVL Tree Rotation Stress ===\n");
    std::ofstream csv("results/raw/avl_adversary.csv");
    csv << "pattern,n,left_rotations,right_rotations,total_rotations,rotations_per_insert,height,time_ms\n";

    std::vector<int> sizes = {1000, 5000, 10000, 25000, 50000, 100000, 250000, 500000};
    for (int n : sizes) {
        AdversaryRun r_mono = runMonotonic(n);
        AdversaryRun r_zig  = runZigzag(n);
        AdversaryRun r_rand = runRandom(n, 12345 + n);

        std::printf("N=%-7d | Monotonic: rot=%-7llu (%.3f/ins, h=%2d, %.1f ms) | Random: rot=%-7llu (%.3f/ins, h=%2d, %.1f ms)\n",
                    n, (unsigned long long)r_mono.total_rot, r_mono.rot_ratio, r_mono.final_height, r_mono.time_ms,
                    (unsigned long long)r_rand.total_rot, r_rand.rot_ratio, r_rand.final_height, r_rand.time_ms);

        auto write_csv = [&](const AdversaryRun& r) {
            csv << r.pattern << "," << r.n << "," << r.left_rot << "," << r.right_rot << ","
                << r.total_rot << "," << r.rot_ratio << "," << r.final_height << "," << r.time_ms << "\n";
        };
        write_csv(r_mono);
        write_csv(r_zig);
        write_csv(r_rand);
    }

    csv.close();
    std::printf("Saved adversarial results to results/raw/avl_adversary.csv\n");
    return 0;
}
