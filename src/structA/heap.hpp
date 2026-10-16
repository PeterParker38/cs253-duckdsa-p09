// heap.hpp -- Structure A: Binary Min-Heap with from-scratch Hash Set lazy cancellation
// Keyed strictly by (time, sequence number).
// Neither the heap nor the hash set uses standard library containers.
#pragma once
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <utility>
#include "../../include/event.hpp"
#include "../../include/event_set.hpp"
#include "custom_hash_set.hpp"

namespace structA {

class CustomMinHeap {
private:
    Event* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;

    void ensureCapacity(std::size_t min_cap) {
        if (capacity_ >= min_cap) return;
        std::size_t new_cap = (capacity_ == 0) ? 16 : capacity_ * 2;
        while (new_cap < min_cap) new_cap *= 2;
        Event* new_data = new Event[new_cap];
        for (std::size_t i = 0; i < size_; i++) {
            new_data[i] = data_[i];
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_cap;
    }

    void siftUp(std::size_t i) {
        while (i > 0) {
            std::size_t p = (i - 1) / 2;
            if (data_[i] < data_[p]) {
                std::swap(data_[i], data_[p]);
                i = p;
            } else {
                break;
            }
        }
    }

    void siftDown(std::size_t i) {
        while (true) {
            std::size_t best = i;
            std::size_t left = 2 * i + 1;
            std::size_t right = 2 * i + 2;

            if (left < size_ && data_[left] < data_[best]) best = left;
            if (right < size_ && data_[right] < data_[best]) best = right;

            if (best == i) break;
            std::swap(data_[i], data_[best]);
            i = best;
        }
    }

public:
    CustomMinHeap() {
        ensureCapacity(16);
    }

    explicit CustomMinHeap(std::size_t initial_cap) {
        ensureCapacity(initial_cap > 0 ? initial_cap : 16);
    }

    ~CustomMinHeap() {
        delete[] data_;
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    CustomMinHeap(const CustomMinHeap& other) {
        ensureCapacity(other.capacity_);
        size_ = other.size_;
        for (std::size_t i = 0; i < size_; i++) data_[i] = other.data_[i];
    }

    CustomMinHeap& operator=(const CustomMinHeap& other) {
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

    CustomMinHeap(CustomMinHeap&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    CustomMinHeap& operator=(CustomMinHeap&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    const Event& peek() const { return data_[0]; }
    const Event& get(std::size_t i) const { return data_[i]; }

    void insert(const Event& e) {
        ensureCapacity(size_ + 1);
        data_[size_] = e;
        siftUp(size_);
        size_++;
    }

    bool deleteMin(Event& out) {
        if (size_ == 0) return false;
        out = data_[0];
        data_[0] = data_[size_ - 1];
        size_--;
        if (size_ > 0) siftDown(0);
        return true;
    }

    void clear() {
        size_ = 0;
    }

    // Floyd's O(n) bottom-up heap construction
    void buildHeap(const Event* src, std::size_t n) {
        ensureCapacity(n);
        for (std::size_t i = 0; i < n; i++) data_[i] = src[i];
        size_ = n;
        if (size_ <= 1) return;
        for (std::size_t i = (size_ / 2); i > 0; i--) {
            siftDown(i - 1);
        }
    }
};

class HeapEventSet : public EventSet {
private:
    CustomMinHeap heap_;
    CustomHashSet cancelled_;
    CustomHashSet in_heap_;
    std::uint64_t purges_ = 0;

    void purge() {
        purges_++;
        std::size_t phys = heap_.size();
        Event* live_buf = new Event[phys];
        std::size_t live_count = 0;

        in_heap_.clear();
        for (std::size_t i = 0; i < phys; i++) {
            const Event& e = heap_.get(i);
            if (!cancelled_.contains(e.seq)) {
                live_buf[live_count++] = e;
                in_heap_.insert(e.seq);
            }
        }

        heap_.buildHeap(live_buf, live_count);
        delete[] live_buf;

        // O(cancelled.size()) clear, resolving the fixed-bucket cost
        cancelled_.clear();
    }

public:
    HeapEventSet() : heap_(64), cancelled_(64), in_heap_(64) {}

    void insert(const Event& e) override {
        if (in_heap_.contains(e.seq)) return;
        heap_.insert(e);
        in_heap_.insert(e.seq);
    }

    bool deleteMin(Event& out) override {
        while (!heap_.empty()) {
            Event cand{};
            heap_.deleteMin(cand);
            in_heap_.erase(cand.seq);

            if (cancelled_.contains(cand.seq)) {
                cancelled_.erase(cand.seq);
                continue; // lazy discard
            }

            out = cand;
            return true;
        }
        return false;
    }

    bool cancel(const Event& e) override {
        if (!in_heap_.contains(e.seq) || cancelled_.contains(e.seq)) {
            return false;
        }

        cancelled_.insert(e.seq);
        std::size_t live = (heap_.size() > cancelled_.size()) ? (heap_.size() - cancelled_.size()) : 0;

        // Purge when cancelled entries strictly outnumber live entries
        if (cancelled_.size() > live) {
            purge();
        }
        return true;
    }

    std::size_t size() const override {
        if (heap_.size() < cancelled_.size()) return 0;
        return heap_.size() - cancelled_.size();
    }

    const char* name() const override { return "heap"; }

    // Successor: first live event with time >= t (smallest (time, seq))
    bool successor(long long t, Event& out) override {
        bool found = false;
        Event best{};
        for (std::size_t i = 0; i < heap_.size(); i++) {
            const Event& e = heap_.get(i);
            if (cancelled_.contains(e.seq)) continue;
            if (e.time >= t) {
                if (!found || e < best) {
                    best = e;
                    found = true;
                }
            }
        }
        if (found) out = best;
        return found;
    }

    std::uint64_t purges() const { return purges_; }
    std::size_t physicalSize() const { return heap_.size(); }
    std::size_t cancelledCount() const { return cancelled_.size(); }
};

} // namespace structA
