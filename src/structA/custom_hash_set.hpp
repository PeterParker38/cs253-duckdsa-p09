// custom_hash_set.hpp -- Structure A from-scratch hash set
// Implemented without standard-library containers.
// Open addressing with linear probing, dynamic resizing, and O(item_count) clearing.
#pragma once
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <utility>

class CustomHashSet {
private:
    struct Slot {
        std::uint64_t key = 0;
        bool occupied = false;
        std::size_t pos_in_indices = 0;
    };

    Slot* table_ = nullptr;
    std::size_t* occupied_indices_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t count_ = 0;

    static inline std::size_t mix64(std::uint64_t x) {
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return (std::size_t)x;
    }

    void allocate(std::size_t cap) {
        capacity_ = cap;
        table_ = new Slot[capacity_];
        occupied_indices_ = new std::size_t[capacity_];
        count_ = 0;
    }

    void deallocate() {
        delete[] table_;
        delete[] occupied_indices_;
        table_ = nullptr;
        occupied_indices_ = nullptr;
        capacity_ = 0;
        count_ = 0;
    }

    void rehash(std::size_t new_cap) {
        Slot* old_table = table_;
        std::size_t* old_indices = occupied_indices_;
        std::size_t old_count = count_;

        capacity_ = new_cap;
        table_ = new Slot[capacity_];
        occupied_indices_ = new std::size_t[capacity_];
        count_ = 0;

        for (std::size_t i = 0; i < old_count; i++) {
            std::size_t idx = old_indices[i];
            if (old_table[idx].occupied) {
                insertNoResize(old_table[idx].key);
            }
        }

        delete[] old_table;
        delete[] old_indices;
    }

    void insertNoResize(std::uint64_t key) {
        std::size_t mask = capacity_ - 1;
        std::size_t idx = mix64(key) & mask;
        while (table_[idx].occupied) {
            if (table_[idx].key == key) return;
            idx = (idx + 1) & mask;
        }
        table_[idx].key = key;
        table_[idx].occupied = true;
        table_[idx].pos_in_indices = count_;
        occupied_indices_[count_] = idx;
        count_++;
    }

public:
    explicit CustomHashSet(std::size_t initial_cap = 16) {
        std::size_t cap = 16;
        while (cap < initial_cap) cap <<= 1;
        allocate(cap);
    }

    ~CustomHashSet() {
        deallocate();
    }

    CustomHashSet(const CustomHashSet& other) {
        allocate(other.capacity_);
        for (std::size_t i = 0; i < other.count_; i++) {
            std::size_t idx = other.occupied_indices_[i];
            insertNoResize(other.table_[idx].key);
        }
    }

    CustomHashSet& operator=(const CustomHashSet& other) {
        if (this != &other) {
            deallocate();
            allocate(other.capacity_);
            for (std::size_t i = 0; i < other.count_; i++) {
                std::size_t idx = other.occupied_indices_[i];
                insertNoResize(other.table_[idx].key);
            }
        }
        return *this;
    }

    CustomHashSet(CustomHashSet&& other) noexcept
        : table_(other.table_),
          occupied_indices_(other.occupied_indices_),
          capacity_(other.capacity_),
          count_(other.count_) {
        other.table_ = nullptr;
        other.occupied_indices_ = nullptr;
        other.capacity_ = 0;
        other.count_ = 0;
    }

    CustomHashSet& operator=(CustomHashSet&& other) noexcept {
        if (this != &other) {
            deallocate();
            table_ = other.table_;
            occupied_indices_ = other.occupied_indices_;
            capacity_ = other.capacity_;
            count_ = other.count_;
            other.table_ = nullptr;
            other.occupied_indices_ = nullptr;
            other.capacity_ = 0;
            other.count_ = 0;
        }
        return *this;
    }

    std::size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    std::size_t capacity() const { return capacity_; }

    bool contains(std::uint64_t key) const {
        if (count_ == 0) return false;
        std::size_t mask = capacity_ - 1;
        std::size_t idx = mix64(key) & mask;
        while (table_[idx].occupied) {
            if (table_[idx].key == key) return true;
            idx = (idx + 1) & mask;
        }
        return false;
    }

    bool insert(std::uint64_t key) {
        if (contains(key)) return false;
        if ((count_ + 1) * 10 >= capacity_ * 6) {
            rehash(capacity_ << 1);
        }
        insertNoResize(key);
        return true;
    }

    bool erase(std::uint64_t key) {
        if (count_ == 0) return false;
        std::size_t mask = capacity_ - 1;
        std::size_t i = mix64(key) & mask;
        while (table_[i].occupied) {
            if (table_[i].key == key) {
                // Remove from occupied_indices_ in O(1)
                std::size_t pos = table_[i].pos_in_indices;
                std::size_t last_idx = occupied_indices_[count_ - 1];
                occupied_indices_[pos] = last_idx;
                table_[last_idx].pos_in_indices = pos;
                count_--;

                // Knuth Algorithm R (backward-shift deletion) to maintain linear probing chains without tombstones
                table_[i].occupied = false;
                std::size_t j = i;
                while (true) {
                    j = (j + 1) & mask;
                    if (!table_[j].occupied) break;
                    std::size_t k = mix64(table_[j].key) & mask;
                    bool between = (i <= j) ? (k <= i || k > j) : (k <= i && k > j);
                    if (between) {
                        table_[i] = table_[j];
                        table_[i].pos_in_indices = table_[j].pos_in_indices;
                        occupied_indices_[table_[i].pos_in_indices] = i;
                        table_[j].occupied = false;
                        i = j;
                    }
                }
                return true;
            }
            i = (i + 1) & mask;
        }
        return false;
    }

    // Resolves fixed-bucket clearing cost: strictly O(count_) time!
    void clear() {
        for (std::size_t i = 0; i < count_; i++) {
            std::size_t idx = occupied_indices_[i];
            table_[idx].occupied = false;
        }
        count_ = 0;
    }
};
