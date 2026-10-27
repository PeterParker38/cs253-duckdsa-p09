// oracle.hpp -- Sorted-Array Oracle with binary-search insertion
// Used as the ground-truth reference for differential testing and shrinking.
#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include "../include/event.hpp"
#include "../include/event_set.hpp"

namespace oracle {

class SortedArrayOracle : public EventSet {
private:
    Event* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;

    void ensureCapacity(std::size_t min_cap) {
        if (capacity_ >= min_cap) return;
        std::size_t new_cap = (capacity_ == 0) ? 16 : capacity_ * 2;
        while (new_cap < min_cap) new_cap *= 2;
        Event* new_data = new Event[new_cap];
        for (std::size_t i = 0; i < size_; i++) new_data[i] = data_[i];
        delete[] data_;
        data_ = new_data;
        capacity_ = new_cap;
    }

    // Binary search for the first element >= (time, seq)
    std::size_t lowerBound(long long t, std::uint64_t s) const {
        std::size_t lo = 0;
        std::size_t hi = size_;
        while (lo < hi) {
            std::size_t mid = lo + (hi - lo) / 2;
            if (data_[mid].time < t || (data_[mid].time == t && data_[mid].seq < s)) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return lo;
    }

public:
    SortedArrayOracle() {
        ensureCapacity(16);
    }

    explicit SortedArrayOracle(std::size_t initial_cap) {
        ensureCapacity(initial_cap > 0 ? initial_cap : 16);
    }

    ~SortedArrayOracle() {
        delete[] data_;
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    SortedArrayOracle(const SortedArrayOracle& other) {
        ensureCapacity(other.capacity_);
        size_ = other.size_;
        for (std::size_t i = 0; i < size_; i++) data_[i] = other.data_[i];
    }

    SortedArrayOracle& operator=(const SortedArrayOracle& other) {
        if (this != &other) {
            delete[] data_;
            data_ = nullptr;
            size_ = 0;
            capacity_ = 0;
            ensureCapacity(other.capacity_);
            size_ = other.size_;
            for (std::size_t i = 0; i < size_; i++) data_[i] = other.data_[i];
        }
        return *this;
    }

    void insert(const Event& e) override {
        std::size_t pos = lowerBound(e.time, e.seq);
        if (pos < size_ && data_[pos].time == e.time && data_[pos].seq == e.seq) {
            return; // duplicate
        }
        ensureCapacity(size_ + 1);
        for (std::size_t i = size_; i > pos; i--) {
            data_[i] = data_[i - 1];
        }
        data_[pos] = e;
        size_++;
    }

    bool deleteMin(Event& out) override {
        if (size_ == 0) return false;
        out = data_[0];
        for (std::size_t i = 0; i + 1 < size_; i++) {
            data_[i] = data_[i + 1];
        }
        size_--;
        return true;
    }

    bool cancel(const Event& e) override {
        std::size_t pos = lowerBound(e.time, e.seq);
        if (pos < size_ && data_[pos].time == e.time && data_[pos].seq == e.seq) {
            for (std::size_t i = pos; i + 1 < size_; i++) {
                data_[i] = data_[i + 1];
            }
            size_--;
            return true;
        }
        return false;
    }

    // Successor: first event with time >= t (smallest (time, seq))
    bool successor(long long t, Event& out) override {
        std::size_t pos = lowerBound(t, 0);
        if (pos < size_) {
            out = data_[pos];
            return true;
        }
        return false;
    }

    std::size_t size() const override { return size_; }
    bool empty() const { return size_ == 0; }
    const char* name() const override { return "oracle"; }

    const Event& get(std::size_t i) const { return data_[i]; }
    void clear() { size_ = 0; }
};

} // namespace oracle
