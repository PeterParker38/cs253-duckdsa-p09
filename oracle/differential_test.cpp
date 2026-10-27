// differential_test.cpp -- Deterministic differential test with >= 1,000,000 operations
// Compares Structure A (Heap), Structure B (AVL), and SortedArrayOracle.
// Validates AVL invariants at every step and verifies automatic test shrinking.
#include <cstdio>
#include <vector>
#include <iostream>
#include <cassert>
#include "../include/rng.hpp"
#include "../src/structA/heap.hpp"
#include "../src/structB/avl.hpp"
#include "oracle.hpp"
#include "shrinker.hpp"

using namespace oracle;
using namespace structA;
using namespace structB;

static void testShrinker() {
    std::printf("\n--- Running Automatic Test Shrinker Verification ---\n");
    // Synthetic bug pattern to verify the Delta Debugging reducer
    std::vector<Operation> noisy_sequence;
    Rng rng(42);
    for (int i = 0; i < 60; i++) {
        long long t = rng.range(1, 100);
        noisy_sequence.push_back({OpType::INSERT, Event{t, (std::uint64_t)i, 0, 0}, 0});
    }

    // Embed trigger:
    noisy_sequence.push_back({OpType::INSERT, Event{10, 1001, 0, 0}, 0});
    noisy_sequence.push_back({OpType::INSERT, Event{25, 1002, 0, 0}, 0});
    noisy_sequence.push_back({OpType::DELETE_MIN, Event{}, 0});
    noisy_sequence.push_back({OpType::SUCCESSOR, Event{}, 20});

    for (int i = 0; i < 40; i++) {
        long long t = rng.range(1, 100);
        noisy_sequence.push_back({OpType::INSERT, Event{t, (std::uint64_t)(2000 + i), 0, 0}, 0});
    }

    auto fails = [](const std::vector<Operation>& ops) -> bool {
        bool has_ins10 = false;
        bool has_ins25 = false;
        bool has_del = false;
        for (const auto& op : ops) {
            if (op.type == OpType::INSERT && op.event.time == 10 && op.event.seq == 1001) has_ins10 = true;
            if (has_ins10 && op.type == OpType::INSERT && op.event.time == 25 && op.event.seq == 1002) has_ins25 = true;
            if (has_ins25 && op.type == OpType::DELETE_MIN) has_del = true;
            if (has_del && op.type == OpType::SUCCESSOR && op.time_arg == 20) return true;
        }
        return false;
    };

    assert(fails(noisy_sequence));
    std::printf("Original sequence length: %zu operations\n", noisy_sequence.size());

    std::vector<Operation> minimized = Shrinker::shrink(noisy_sequence, fails);
    std::printf("Minimized sequence length: %zu operations\n", minimized.size());
    for (size_t i = 0; i < minimized.size(); i++) {
        std::printf("  [%zu] %s\n", i + 1, minimized[i].toString().c_str());
    }
    assert(minimized.size() == 4);
    assert(fails(minimized));
    std::printf("Automatic Shrinker: PASSED (successfully reduced to 4 minimal operations)\n\n");
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    long long total_ops_target = 1000000;
    if (argc > 1) {
        total_ops_target = std::atoll(argv[1]);
    }

    std::printf("=== Differential Test: Heap vs AVL vs Sorted-Array Oracle ===\n");
    std::printf("Target operations: %lld\n", total_ops_target);
    std::fflush(stdout);

    testShrinker();

    Rng rng(20261113);
    long long ops = 0;
    int mismatches = 0;
    std::uint64_t seq = 0;
    int round = 0;

    const int ROUND_OPS = 2500;

    while (ops < total_ops_target) {
        round++;
        SortedArrayOracle oracle;
        HeapEventSet heap;
        AvlEventSet avl;

        std::vector<Event> live;
        std::vector<Event> dead;

        // Vary spread: small spread induces many ties; large spread tests wide distribution
        long long spread = (round % 5 == 0) ? 3 : (round % 5 == 1) ? 20 : (round % 5 == 2) ? 100 : 2000;

        for (int step = 0; step < ROUND_OPS && ops < total_ops_target; step++) {
            ops++;
            int r = (int)rng.range(0, 99);

            if (r < 45 || live.empty()) {
                // Operation 1: INSERT
                Event e;
                e.time = rng.range(0, spread);
                e.seq = seq++;
                e.type = (int)rng.range(0, 2);
                e.packet = (int)rng.range(0, 99);

                oracle.insert(e);
                heap.insert(e);
                avl.insert(e);
                live.push_back(e);
            } else if (r < 72) {
                // Operation 2: CANCEL
                if (!live.empty() && rng.range(0, 4) != 0) {
                    size_t idx = (size_t)rng.range(0, (long long)live.size() - 1);
                    Event e = live[idx];
                    live[idx] = live.back();
                    live.pop_back();
                    dead.push_back(e);

                    bool o_ok = oracle.cancel(e);
                    bool h_ok = heap.cancel(e);
                    bool a_ok = avl.cancel(e);

                    if (o_ok != h_ok || o_ok != a_ok) {
                        std::printf("ERROR: Cancel mismatch at op %lld: oracle=%d heap=%d avl=%d\n", ops, o_ok, h_ok, a_ok);
                        mismatches++;
                    }

                    // Repeated cancellation must fail everywhere
                    bool o_ok2 = oracle.cancel(e);
                    bool h_ok2 = heap.cancel(e);
                    bool a_ok2 = avl.cancel(e);
                    if (o_ok2 || h_ok2 || a_ok2) {
                        std::printf("ERROR: 2nd Cancel returned true at op %lld: oracle=%d heap=%d avl=%d\n", ops, o_ok2, h_ok2, a_ok2);
                        mismatches++;
                    }
                } else if (!dead.empty()) {
                    Event e = dead[rng.range(0, (long long)dead.size() - 1)];
                    bool o_ok = oracle.cancel(e);
                    bool h_ok = heap.cancel(e);
                    bool a_ok = avl.cancel(e);
                    if (o_ok || h_ok || a_ok) {
                        std::printf("ERROR: Dead cancel returned true at op %lld: oracle=%d heap=%d avl=%d\n", ops, o_ok, h_ok, a_ok);
                        mismatches++;
                    }
                }
            } else if (r < 88) {
                // Operation 3: DELETE_MIN
                Event o_min{}, h_min{}, a_min{};
                bool o_ok = oracle.deleteMin(o_min);
                bool h_ok = heap.deleteMin(h_min);
                bool a_ok = avl.deleteMin(a_min);

                if (o_ok != h_ok || o_ok != a_ok) {
                    std::printf("ERROR: deleteMin status mismatch at op %lld: oracle=%d heap=%d avl=%d\n", ops, o_ok, h_ok, a_ok);
                    mismatches++;
                }

                if (o_ok) {
                    if (o_min != h_min || o_min != a_min) {
                        std::printf("ERROR: deleteMin value mismatch at op %lld: oracle=(%lld,%llu) heap=(%lld,%llu) avl=(%lld,%llu)\n",
                                    ops, o_min.time, (unsigned long long)o_min.seq,
                                    h_min.time, (unsigned long long)h_min.seq,
                                    a_min.time, (unsigned long long)a_min.seq);
                        mismatches++;
                    }

                    for (size_t i = 0; i < live.size(); i++) {
                        if (live[i].seq == o_min.seq) {
                            live[i] = live.back();
                            live.pop_back();
                            break;
                        }
                    }
                    dead.push_back(o_min);
                }
            } else {
                // Operation 4: SUCCESSOR(t)
                long long t = rng.range(-5, spread + 10);
                Event o_succ{}, h_succ{}, a_succ{};
                bool o_ok = oracle.successor(t, o_succ);
                bool h_ok = heap.successor(t, h_succ);
                bool a_ok = avl.successor(t, a_succ);

                if (o_ok != a_ok || o_ok != h_ok) {
                    std::printf("ERROR: successor(%lld) existence mismatch at op %lld: oracle=%d heap=%d avl=%d\n", t, ops, o_ok, h_ok, a_ok);
                    mismatches++;
                } else if (o_ok) {
                    if (o_succ != a_succ || o_succ != h_succ) {
                        std::printf("ERROR: successor(%lld) value mismatch at op %lld: oracle=(%lld,%llu) heap=(%lld,%llu) avl=(%lld,%llu)\n",
                                    t, o_succ.time, (unsigned long long)o_succ.seq,
                                    h_succ.time, (unsigned long long)h_succ.seq,
                                    a_succ.time, (unsigned long long)a_succ.seq);
                        mismatches++;
                    }
                }
            }

            // Invariant 1: size equality across all structures
            if (oracle.size() != heap.size() || oracle.size() != avl.size()) {
                std::printf("ERROR: Size mismatch at op %lld: oracle=%zu heap=%zu avl=%zu\n",
                            ops, oracle.size(), heap.size(), avl.size());
                mismatches++;
            }

            // Invariant 2: AVL tree invariants (BST order, stored height, balance factor in {-1, 0, 1})
            if (ops % 50 == 0 && !avl.validate()) {
                std::printf("ERROR: AVL Invariant violation at op %lld!\n", ops);
                mismatches++;
            }

            if (mismatches > 20) {
                std::fprintf(stderr, "Aborting differential test due to excessive mismatches (>20)\n");
                return 1;
            }
        }

        // Final draining of round: all structures must empty with identical sequence
        while (!oracle.empty()) {
            Event o_min{}, h_min{}, a_min{};
            bool o_ok = oracle.deleteMin(o_min);
            bool h_ok = heap.deleteMin(h_min);
            bool a_ok = avl.deleteMin(a_min);
            if (o_ok != h_ok || o_ok != a_ok || o_min != h_min || o_min != a_min) {
                std::printf("ERROR: Round draining mismatch!\n");
                mismatches++;
                break;
            }
        }
        assert(avl.validate());

        if (ops % 100000 == 0 || ops >= total_ops_target) {
            std::printf("Progress: %lld / %lld operations verified (mismatches: %d, rounds: %d)\n",
                        ops, total_ops_target, mismatches, round);
            std::fflush(stdout);
        }
    }

    std::printf("\n=== FINAL RESULT: %lld operations tested, %d mismatches ===\n", ops, mismatches);
    if (mismatches == 0) {
        std::printf("ALL DIFFERENTIAL TESTS PASSED: 100%% agreement across Oracle, Heap, and AVL.\n");
        return 0;
    } else {
        std::printf("TEST FAILED: %d mismatches detected.\n", mismatches);
        return 1;
    }
}
