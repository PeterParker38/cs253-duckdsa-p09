// shrinker.hpp -- Automatic test-case reducer for failing operation sequences
// Uses Delta Debugging (ddmin) to shrink a failure sequence to a 1-minimal reproducible case.
#pragma once
#include <vector>
#include <functional>
#include <iostream>
#include <string>
#include "../include/event.hpp"

namespace oracle {

enum class OpType { INSERT, DELETE_MIN, CANCEL, SUCCESSOR };

struct Operation {
    OpType type;
    Event event;
    long long time_arg = 0;

    std::string toString() const {
        switch (type) {
            case OpType::INSERT:
                return "INSERT(time=" + std::to_string(event.time) +
                       ", seq=" + std::to_string(event.seq) + ")";
            case OpType::DELETE_MIN:
                return "DELETE_MIN()";
            case OpType::CANCEL:
                return "CANCEL(time=" + std::to_string(event.time) +
                       ", seq=" + std::to_string(event.seq) + ")";
            case OpType::SUCCESSOR:
                return "SUCCESSOR(t=" + std::to_string(time_arg) + ")";
        }
        return "UNKNOWN";
    }
};

class Shrinker {
public:
    using FailurePredicate = std::function<bool(const std::vector<Operation>&)>;

    // Shrinks a failing operation sequence to a minimal reproducible sequence using ddmin
    static std::vector<Operation> shrink(const std::vector<Operation>& initial,
                                         FailurePredicate fails) {
        if (!fails(initial)) {
            // Initial sequence did not fail
            return initial;
        }

        std::vector<Operation> current = initial;
        std::size_t n = 2;

        while (current.size() >= 2 && n <= current.size()) {
            bool reduced = false;
            std::size_t chunk_size = (current.size() + n - 1) / n;

            // Try removing each chunk (testing complement)
            for (std::size_t i = 0; i < n; i++) {
                std::size_t start = i * chunk_size;
                if (start >= current.size()) break;
                std::size_t end = std::min(start + chunk_size, current.size());

                std::vector<Operation> complement;
                complement.reserve(current.size() - (end - start));
                for (std::size_t j = 0; j < start; j++) complement.push_back(current[j]);
                for (std::size_t j = end; j < current.size(); j++) complement.push_back(current[j]);

                if (!complement.empty() && fails(complement)) {
                    current = std::move(complement);
                    n = std::max<std::size_t>(n - 1, 2);
                    reduced = true;
                    break;
                }
            }
            if (reduced) continue;

            // Try testing each chunk directly
            for (std::size_t i = 0; i < n; i++) {
                std::size_t start = i * chunk_size;
                if (start >= current.size()) break;
                std::size_t end = std::min(start + chunk_size, current.size());

                std::vector<Operation> chunk;
                chunk.reserve(end - start);
                for (std::size_t j = start; j < end; j++) chunk.push_back(current[j]);

                if (!chunk.empty() && fails(chunk)) {
                    current = std::move(chunk);
                    n = 2;
                    reduced = true;
                    break;
                }
            }
            if (reduced) continue;

            if (n >= current.size()) break;
            n = std::min(n * 2, current.size());
        }

        // Final single-pass 1-minimality check
        bool changed = true;
        while (changed && current.size() > 1) {
            changed = false;
            for (std::size_t i = 0; i < current.size(); i++) {
                std::vector<Operation> candidate;
                candidate.reserve(current.size() - 1);
                for (std::size_t j = 0; j < current.size(); j++) {
                    if (j != i) candidate.push_back(current[j]);
                }
                if (!candidate.empty() && fails(candidate)) {
                    current = std::move(candidate);
                    changed = true;
                    break;
                }
            }
        }

        return current;
    }
};

} // namespace oracle
