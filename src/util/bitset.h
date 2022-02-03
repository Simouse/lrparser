//
// Created by Simon Yu on 2022/1/14.
//

#ifndef LRPARSER_BITSET_H
#define LRPARSER_BITSET_H

#include <array>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <cstring>

#include "src/common.h"

namespace util {

// A bitset whose size can only grow. The index is started from 0, and capacity
// is rounded to multiple of `block_bits` bits.
// Template argument T is used for type check.
template <class T> class BitSet {
  public:
    using block_type = unsigned int;
    using block_signed_type = int;
    using size_type = ::size_t;
    static constexpr size_type block_bits = sizeof(block_type) * 8;
    static constexpr size_type npos = static_cast<size_type>(-1);
    static constexpr block_type mask_all_ones = static_cast<block_type>(-1);
    static constexpr size_type n_inner_blocks = 2;
    friend struct std::hash<util::BitSet<T>>;

    static_assert(std::is_integral_v<T> || std::is_enum_v<T>,
                  "BitSet can only hold integral types");

  private:
    // **Initializing all data here is much safer**

    // Inner blocks (for small bitset)
    std::array<block_type, n_inner_blocks> inner_blocks = {0};

    // Data pointer
    block_type *m_data = &inner_blocks[0];

    // Capacity in blocks
    size_type m_size = n_inner_blocks;

    // Prerequisite: m_size must be set.
    void allocMemory(size_type size, bool setBitsToZeros = true) {
        m_data =
            (size > n_inner_blocks) ? new block_type[size] : &inner_blocks[0];
        m_size = size;
        if (!setBitsToZeros)
            return;
        fillZeros(m_data, 0, size);
    }

    void freeMemory() {
        if (m_data && m_data != &inner_blocks[0]) {
            assert(m_size > n_inner_blocks);

            delete[] m_data;
            m_data = nullptr;
        }
    }

    static size_type roundToNextPowerOf2(size_type i) {
        if (i <= 2)
            return i;
        --i;
        i |= i >> 1;
        i |= i >> 2;
        i |= i >> 4;
        i |= i >> 8;
        i |= i >> 16;
        i |= i >> 32;
        return 1 + i;
    }

    // Fill data to 0 in range [from, to).
    static inline void fillZeros(block_type *data, size_type from,
                                 size_type to) {
        for (size_type i = from; i < to; ++i)
            data[i] = 0;
    }

    static inline void copyRange(block_type *dest, const block_type *src,
                                 size_type from, size_type to) {
        for (size_type i = from; i < to; ++i)
            dest[i] = src[i];
    }

    // Copy every data member (treat them as trivial values) from `other`.
    // After that, `m_data` is modified to point to `&inner_blocks[0]` if
    // small bitset optimization is possible.
    void copyContent(BitSet const &other) {
        using ClassType = typename std::remove_reference<decltype(*this)>::type;
        using CastType = std::array<unsigned char, sizeof(ClassType)>;
        *(CastType *)this = *(CastType *)&other;

        if (other.m_data == &other.inner_blocks[0]) {
            this->m_data = &inner_blocks[0];
        }
    }

  public:
    explicit BitSet(size_type N)
        : inner_blocks({0}),
          m_size(roundToNextPowerOf2((N + block_bits - 1) / block_bits)) {
        // Make sure m_size is at least n_inner_blocks
        if (m_size < n_inner_blocks) {
            m_size = n_inner_blocks;
        }
        // Initialize m_data
        allocMemory(m_size, true);
    }

    BitSet() = default; // All data is initialized in definition

    BitSet(BitSet const &other) {
        copyContent(other);
        // 1. Do not release memory in `m_data`, it belongs to `other`.
        // 2. Now this->m_size == other.m_size.
        allocMemory(m_size, false);
        copyRange(m_data, other.m_data, 0, m_size);
    }

    BitSet(BitSet &&other) noexcept {
        copyContent(other);
        other.m_data = nullptr;
    }

    BitSet &operator=(BitSet const &other) {
        // Self-assignment check
        if (this == &other)
            return *this;

        freeMemory();
        copyContent(other);
        allocMemory(other.m_size, true);
        copyRange(m_data, other.m_data, 0, other.m_size);
        return *this;
    }

    BitSet &operator=(BitSet &&other) noexcept {
        if (this == &other)
            return *this;

        freeMemory();
        copyContent(other);
        other.m_data = nullptr;

        return *this;
    }

    ~BitSet() { freeMemory(); }

    // Set N-th bit. If dynamical expansion is needed, use ensure() first.
    // Only set() and insert() need to ensure that T >= 0.
    void set(size_type N, bool flag = true) {
        assert(N < m_size * block_bits);
        if (flag)
            m_data[N / block_bits] |= 1LLU << (N % block_bits);
        else
            m_data[N / block_bits] &= ~(1LLU << (N % block_bits));
    }

    // For remove() and contains(), if N < 0, function will
    // return immediately, so no error can happen.
    void remove(T N_) {
        auto N = static_cast<size_type>(N_);
        if (N < m_size * block_bits) {
            set(N, false);
        }
    }

    // Insert a number which is no less than 0. ensure() is called
    // automatically. See set().
    void insert(T N_) {
        assert(N_ >= 0);
        auto N = static_cast<size_type>(N_);
        ensure(N + 1);
        set(N, true);
    }

    // Test if N-th bit exists and is set to true. See remove().
    [[nodiscard]] bool contains(T N_) const {
        auto N = static_cast<size_type>(N_);
        if (N >= m_size * block_bits)
            return false;
        return m_data[N / block_bits] & (1LLU << (N % block_bits));
    }

    // Clear all bits and make bitset usable again (even if it was moved).
    void clear() {
        // If bitset data is corrupted (moved), allocate new memory for it
        if (!m_data) {
            allocMemory(m_size, false);
        }
        // Clear memory
        for (size_type i = 0; i < m_size; ++i)
            m_data[i] = 0;
    }

    // Ensure bitset can hold at least N bits.
    void ensure(size_type N) {
        size_type leastBlocksNeeded = ((N + block_bits - 1) / block_bits);
        if (leastBlocksNeeded <= m_size) {
            return;
        }
        auto prevData = m_data;
        auto prevSize = m_size;
        size_type capacity = roundToNextPowerOf2(leastBlocksNeeded);
        // Because m_size is always larger than or equal to n_inner_blocks, and
        // leastBlocksNeeded > m_size now, we know that inner blocks cannot be
        // used. allocMemory() will definitely request a dynamically allocated
        // memory space.
        allocMemory(capacity, false);
        copyRange(m_data, prevData, 0, prevSize);
        fillZeros(m_data, prevSize, m_size);
        if (prevData != &inner_blocks[0]) {
            delete[] prevData;
        }
    }

    bool operator==(BitSet const &other) const {
        BitSet const *smaller = this;
        BitSet const *larger = &other;
        if (smaller->m_size > larger->m_size) {
            std::swap(smaller, larger);
        }
        size_type sz = smaller->m_size;
        for (size_type i = 0; i < sz; ++i) {
            if (m_data[i] != other.m_data[i])
                return false;
        }
        for (size_type i = sz; i < larger->m_size; ++i) {
            if (larger->m_data[i])
                return false;
        }
        return true;
    }

    BitSet &operator&=(BitSet const &other) {
        ensure(other.m_size * block_bits);
        for (size_type i = 0; i < other.m_size; ++i) {
            m_data[i] &= other.m_data[i];
        }
        return *this;
    }

    BitSet &operator|=(BitSet const &other) {
        ensure(other.m_size * block_bits);
        for (size_type i = 0; i < other.m_size; ++i) {
            m_data[i] |= other.m_data[i];
        }
        return *this;
    }

    BitSet &operator^=(BitSet const &other) {
        ensure(other.m_size * block_bits);
        for (size_type i = 0; i < other.m_size; ++i) {
            m_data[i] ^= other.m_data[i];
        }
        return *this;
    }

    // Test if two bitsets have intersection
    [[nodiscard]] bool hasIntersection(BitSet const &other) const {
        auto min_size = m_size < other.m_size ? m_size : other.m_size;
        for (size_type i = 0; i < min_size; ++i) {
            if (m_data[i] & other.m_data[i])
                return true;
        }
        return false;
    }

    // Dump this bitset in human-readable format.
    [[nodiscard]] std::string dump() const {
        std::string s = "{";
        bool flag = false;
        for (auto i : *this) {
            if (flag)
                s += ", ";
            s += std::to_string(i);
            flag = true;
        }
        s += '}';
        return s;
    }

    struct Iterator {
      private:
        BitSet const &bitset;
        block_type mask;
        size_type index; // Needs to work with mask
        size_type availablePos;

        bool advance() {
            if (index == npos)
                return false;

            block_type x = 0;
            while (index < bitset.m_size &&
                   !(x = bitset.m_data[index] & mask)) {
                ++index;
                mask = mask_all_ones;
            }

            if (!x) {
                availablePos = npos;
                index = npos;
                mask = mask_all_ones;
                return false;
            }

            x = x & (block_type)(-(block_signed_type)x); // Get right most bit
            mask &= ~x; // Set mask for further use

            size_type pos = block_bits - 1;
            if (x & 0x0000'ffff)
                pos -= 16;
            if (x & 0x00ff'00ff)
                pos -= 8;
            if (x & 0x0f0f'0f0f)
                pos -= 4;
            if (x & 0x3333'3333)
                pos -= 2;
            if (x & 0x5555'5555)
                --pos;

            // Add offset to global position
            pos = index * block_bits + pos;

            availablePos = pos;
            return true;
        }

      public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;

        explicit Iterator(BitSet const &_bitset, bool end = false)
            : bitset(_bitset), mask(mask_all_ones), index(end ? npos : 0),
              availablePos(npos) {
            if (!end)
                advance();
        }

        Iterator &operator++() {
            advance();
            return *this;
        }

        value_type operator*() const { return static_cast<T>(availablePos); }

        friend bool operator!=(const Iterator &a, const Iterator &b) {
            return &a.bitset != &b.bitset || a.index != b.index ||
                   a.mask != b.mask;
        }
    };

    [[nodiscard]] Iterator begin() const { return Iterator(*this); }
    [[nodiscard]] Iterator end() const { return Iterator(*this, true); }
};

} // namespace util

namespace std {
template <class T> struct hash<util::BitSet<T>> {
    std::size_t operator()(util::BitSet<T> const &bitset) const {
        using std::hash;
        std::size_t res = 17;
        for (typename util::BitSet<T>::size_type i = 0; i < bitset.m_size;
             ++i) {
            res = res * 31 + hash<typename util::BitSet<T>::block_type>()(
                                 bitset.m_data[i]);
        }
        return res;
    }
};
} // namespace std

#endif // LRPARSER_BITSET_H
