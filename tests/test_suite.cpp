// tests/test_suite.cpp -- Focused unit and regression test suite
// Covers:
// 1. Same-time ties and sequence-number ordering
// 2. Cancellation: valid cancel, repeated (double) cancel, non-existent cancel, cancel after pop
// 3. Purge threshold trigger (cancelled > live) in Structure A
// 4. Successor boundary and tie-breaking semantics
// 5. Empty structure behavior
// 6. Agreement across Heap, AVL, and Oracle
#include <cstdio>
#include <cassert>
#include <vector>
#include <memory>
#include <iostream>
#include "../include/event.hpp"
#include "../src/structA/heap.hpp"
#include "../src/structB/avl.hpp"
#include "../oracle/oracle.hpp"
#include "../include/simulator.hpp"

using namespace structA;
using namespace structB;
using namespace oracle;

static int total_tests = 0;
static int passed_tests = 0;

#define TEST_ASSERT(cond, msg) do { \
    total_tests++; \
    if (cond) { \
        passed_tests++; \
        std::printf("  [PASS] %s\n", msg); \
    } else { \
        std::printf("  [FAIL] %s (Line %d: %s)\n", msg, __LINE__, #cond); \
    } \
} while (0)

void testTiesAndOrdering() {
    std::printf("\n=== Test Suite 1: Ties and Ordering ===\n");
    HeapEventSet heap;
    AvlEventSet avl;
    SortedArrayOracle oracle;
    EventSet* sets[3] = { &heap, &avl, &oracle };

    // Insert 5 events with SAME timestamp 50, but out-of-order sequence numbers
    std::vector<std::uint64_t> seqs = {42, 10, 88, 5, 23};
    for (std::uint64_t s : seqs) {
        Event e{50, s, 0, 0};
        for (auto* set : sets) set->insert(e);
    }

    // Must come out in strictly increasing sequence number: 5, 10, 23, 42, 88
    std::vector<std::uint64_t> expected = {5, 10, 23, 42, 88};
    for (std::uint64_t exp_s : expected) {
        for (auto* set : sets) {
            Event out{};
            bool ok = set->deleteMin(out);
            TEST_ASSERT(ok && out.time == 50 && out.seq == exp_s,
                        (std::string(set->name()) + ": same-time tie resolved by seq=" + std::to_string(exp_s)).c_str());
        }
    }

    // Timestamp priority: smaller time wins even if sequence number is much larger
    Event e_early{10, 9999, 0, 0};
    Event e_late{20, 1, 0, 0};
    for (auto* set : sets) {
        set->insert(e_late);
        set->insert(e_early);
        Event out{};
        set->deleteMin(out);
        TEST_ASSERT(out.time == 10 && out.seq == 9999,
                    (std::string(set->name()) + ": earlier timestamp wins over smaller sequence number").c_str());
        set->deleteMin(out);
    }
}

void testCancellations() {
    std::printf("\n=== Test Suite 2: Cancellation Edge Cases ===\n");
    HeapEventSet heap;
    AvlEventSet avl;
    SortedArrayOracle oracle;
    EventSet* sets[3] = { &heap, &avl, &oracle };

    Event e1{100, 1, 0, 0};
    Event e2{200, 2, 0, 0};
    Event e3{300, 3, 0, 0};

    for (auto* set : sets) {
        set->insert(e1);
        set->insert(e2);
        set->insert(e3);
    }

    // 1. Valid cancellation of middle event
    for (auto* set : sets) {
        bool ok = set->cancel(e2);
        TEST_ASSERT(ok, (std::string(set->name()) + ": valid cancellation of live event e2").c_str());
        TEST_ASSERT(set->size() == 2, (std::string(set->name()) + ": size reduced to 2").c_str());
    }

    // 2. Repeated (double) cancellation must return false
    for (auto* set : sets) {
        bool ok = set->cancel(e2);
        TEST_ASSERT(!ok, (std::string(set->name()) + ": repeated cancel of e2 returns false").c_str());
        TEST_ASSERT(set->size() == 2, (std::string(set->name()) + ": size unchanged after repeated cancel").c_str());
    }

    // 3. Cancelling non-existent event
    Event non_existent{500, 99, 0, 0};
    for (auto* set : sets) {
        bool ok = set->cancel(non_existent);
        TEST_ASSERT(!ok, (std::string(set->name()) + ": cancelling non-existent event returns false").c_str());
    }

    // 4. Pop min (e1) then try to cancel it
    for (auto* set : sets) {
        Event out{};
        set->deleteMin(out);
        TEST_ASSERT(out.seq == e1.seq, (std::string(set->name()) + ": popped e1").c_str());
        bool ok = set->cancel(e1);
        TEST_ASSERT(!ok, (std::string(set->name()) + ": cancelling already-popped event returns false").c_str());
    }
}

void testHeapPurgeThreshold() {
    std::printf("\n=== Test Suite 3: Structure A Purge Policy Invariant ===\n");
    HeapEventSet heap;

    // Insert 10 events: live = 10, cancelled = 0
    std::vector<Event> events;
    for (int i = 0; i < 10; i++) {
        Event e{100 + i, (std::uint64_t)i, 0, 0};
        heap.insert(e);
        events.push_back(e);
    }
    TEST_ASSERT(heap.purges() == 0, "No purges after insertions");

    // Cancel 5 events: cancelled = 5, live = 5.
    // Condition is cancelled > live: 5 > 5 is FALSE, so purges should STILL be 0!
    for (int i = 0; i < 5; i++) {
        heap.cancel(events[i]);
    }
    TEST_ASSERT(heap.purges() == 0, "Purge NOT triggered when cancelled == live (5 == 5)");
    TEST_ASSERT(heap.physicalSize() == 10, "Physical size remains 10 (unpurged)");

    // Cancel 6th event: cancelled = 6, live = 4.
    // 6 > 4 is TRUE: Purge MUST trigger immediately!
    heap.cancel(events[5]);
    TEST_ASSERT(heap.purges() == 1, "Purge triggered immediately when cancelled > live (6 > 4)");
    TEST_ASSERT(heap.physicalSize() == 4, "Physical size compacted to exactly live size (4)");
    TEST_ASSERT(heap.cancelledCount() == 0, "Cancelled count reset to 0 after purge");
    TEST_ASSERT(heap.size() == 4, "Live size correctly reported as 4");
}

void testSuccessorSemantics() {
    std::printf("\n=== Test Suite 4: Successor(t) Definition and Boundary Cases ===\n");
    // Definition: successor(t) returns the first event with time >= t
    AvlEventSet avl;
    SortedArrayOracle oracle;
    HeapEventSet heap;

    Event e10{10, 1, 0, 0};
    Event e20a{20, 2, 0, 0};
    Event e20b{20, 5, 0, 0};
    Event e30{30, 3, 0, 0};

    for (EventSet* s : { (EventSet*)&avl, (EventSet*)&oracle, (EventSet*)&heap }) {
        s->insert(e10);
        s->insert(e20a);
        s->insert(e20b);
        s->insert(e30);
    }

    for (EventSet* s : { (EventSet*)&avl, (EventSet*)&oracle, (EventSet*)&heap }) {
        Event out{};

        // 1. Query before minimum: t=5 -> returns e10
        bool ok = s->successor(5, out);
        TEST_ASSERT(ok && out.time == 10 && out.seq == 1,
                    (std::string(s->name()) + ": successor(5) returns e10").c_str());

        // 2. Exact match at minimum: t=10 -> returns e10 (since time >= 10)
        ok = s->successor(10, out);
        TEST_ASSERT(ok && out.time == 10 && out.seq == 1,
                    (std::string(s->name()) + ": successor(10) returns e10 (includes t)").c_str());

        // 3. Intermediate query: t=15 -> returns e20a (smallest seq at time 20)
        ok = s->successor(15, out);
        TEST_ASSERT(ok && out.time == 20 && out.seq == 2,
                    (std::string(s->name()) + ": successor(15) returns e20a").c_str());

        // 4. Exact match with tie: t=20 -> returns e20a (seq 2 < seq 5)
        ok = s->successor(20, out);
        TEST_ASSERT(ok && out.time == 20 && out.seq == 2,
                    (std::string(s->name()) + ": successor(20) returns e20a (ties broken by seq)").c_str());

        // 5. Query at maximum: t=30 -> returns e30
        ok = s->successor(30, out);
        TEST_ASSERT(ok && out.time == 30 && out.seq == 3,
                    (std::string(s->name()) + ": successor(30) returns e30").c_str());

        // 6. Query beyond maximum: t=35 -> returns false
        ok = s->successor(35, out);
        TEST_ASSERT(!ok, (std::string(s->name()) + ": successor(35) returns false (no element >= 35)").c_str());
    }
}

void testEmptyStructures() {
    std::printf("\n=== Test Suite 5: Empty Structure Robustness ===\n");
    HeapEventSet heap;
    AvlEventSet avl;
    SortedArrayOracle oracle;
    EventSet* sets[3] = { &heap, &avl, &oracle };

    for (auto* set : sets) {
        Event out{};
        TEST_ASSERT(set->size() == 0, (std::string(set->name()) + ": initial size is 0").c_str());
        TEST_ASSERT(!set->deleteMin(out), (std::string(set->name()) + ": deleteMin on empty returns false").c_str());
        TEST_ASSERT(!set->cancel(Event{10, 1, 0, 0}), (std::string(set->name()) + ": cancel on empty returns false").c_str());
        TEST_ASSERT(!set->successor(10, out), (std::string(set->name()) + ": successor on empty returns false").c_str());
    }
}

void testSimulationEquivalence() {
    std::printf("\n=== Test Suite 6: Full Network Simulation Equivalence (1M Events) ===\n");
    SimConfig cfg;
    cfg.maxEvents = 1000000;
    cfg.seed = 12345;

    HeapEventSet heap;
    AvlEventSet avl;

    Simulator sim_heap(cfg);
    SimResult res_heap = sim_heap.run(heap);

    Simulator sim_avl(cfg);
    SimResult res_avl = sim_avl.run(avl);

    TEST_ASSERT(res_heap.traceHash == res_avl.traceHash, "1M simulation: traceHash identical");
    TEST_ASSERT(res_heap.processed == res_avl.processed, "1M simulation: processed count identical");
    TEST_ASSERT(res_heap.scheduled == res_avl.scheduled, "1M simulation: scheduled count identical");
    TEST_ASSERT(res_heap.cancelled == res_avl.cancelled, "1M simulation: cancelled count identical");
    TEST_ASSERT(res_heap.retransmissions == res_avl.retransmissions, "1M simulation: retransmissions identical");
    TEST_ASSERT(res_heap.maxLive == res_avl.maxLive, "1M simulation: maxLive identical");
    TEST_ASSERT(res_heap.lastTime == res_avl.lastTime, "1M simulation: lastTime identical");

    std::printf("  Simulation Summary: processed=%llu, cancelled=%llu (%.1f%%), traceHash=%016llx\n",
                (unsigned long long)res_heap.processed, (unsigned long long)res_heap.cancelled,
                100.0 * res_heap.cancelled / res_heap.scheduled,
                (unsigned long long)res_heap.traceHash);
}

int main() {
    std::printf("Running Comprehensive Test Suite...\n");
    testTiesAndOrdering();
    testCancellations();
    testHeapPurgeThreshold();
    testSuccessorSemantics();
    testEmptyStructures();
    testSimulationEquivalence();

    std::printf("\n===================================================\n");
    std::printf("Test Suite Results: %d / %d tests passed\n", passed_tests, total_tests);
    std::printf("===================================================\n");

    return (passed_tests == total_tests) ? 0 : 1;
}
