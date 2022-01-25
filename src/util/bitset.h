//
// Created by Simon Yu on 2022/1/14.
//

#ifndef LRPARSER_BITSET_H
#define LRPARSER_BITSET_H

#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>

#include "src/common.h"

namespace util {

// A bit set whose size can be given in runtime but once size
// is given, it's fixed. The index is started from 0, and capacity
// is rounded to multiple of `block_bits` bits.
class BitSet {
   public:
    using block_type = unsigned int;
    using block_signed_type = int;
    using size_type = ::size_t;
    static constexpr size_type block_bits = sizeof(block_type) * 8;
    static constexpr size_type npos = static_cast<size_type>(-1);
    static constexpr block_type mask_all_ones = static_cast<block_type>(-1);
    static constexpr size_type n_inner_blocks = 2;
    friend struct std::hash<util::BitSet>;

   private:
    // **Initializing all data here is much much safer**
    // Default constructor is of no use to copy-assignment in containers
    std::array<block_type, n_inner_blocks> inner_blocks = {0};
    block_type *m_data = &inner_blocks[0];
    size_type m_size = n_inner_blocks;
    size_type nbits = n_inner_blocks * block_bits;

    // Prerequisite: m_size must be set.
    inline void allocMemory(size_type size, bool setBitsToZeros = true);
    inline void freeMemory();

   public:
    explicit BitSet(size_type N);
    BitSet() = delete;
    BitSet(BitSet const &other);
    BitSet(BitSet &&other) noexcept;
    BitSet &operator=(BitSet const &other);
    BitSet &operator=(BitSet &&other) noexcept;
    ~BitSet();
    void set(size_type N, bool flag = true);
    void add(size_type N);
    void remove(size_type N);
    [[nodiscard]] bool contains(size_type N) const;
    void clear();
    bool operator==(BitSet const &other) const;

    // No safety check. Use at risk.
    BitSet &operator&=(BitSet const &other);
    // No safety check. Use at risk.
    BitSet &operator|=(BitSet const &other);
    // No safety check. Use at risk.
    BitSet &operator^=(BitSet const &other);
    // No safety check. Use at risk.
    [[nodiscard]] bool hasIntersection(BitSet const &other) const;

    struct Iterator {
       private:
        BitSet const &bitset;
        block_type mask;
        size_type index;  // Needs to work with mask
        size_type availablePos;
        bool advance();

       public:
        using iterator_category = std::input_iterator_tag;
        using value_type = size_type;
        explicit Iterator(BitSet const &_bitset, bool end = false);
        Iterator &operator++();
        value_type operator*() const;
        friend bool operator!=(const Iterator &a, const Iterator &b);
    };

    [[nodiscard]] Iterator begin() const { return Iterator(*this); }
    [[nodiscard]] Iterator end() const { return Iterator(*this, true); }
    [[nodiscard]] String dump() const;
};

}  // namespace util

namespace std {
template <>
struct hash<util::BitSet> {
    std::size_t operator()(util::BitSet const &bitset) const {
        using std::hash;
        std::size_t res = 17;
        for (util::BitSet::size_type i = 0; i < bitset.m_size; ++i) {
            res = res * 31 + hash<util::BitSet::block_type>()(bitset.m_data[i]);
        }
        return res;
    }
};
}  // namespace std

#endif  // LRPARSER_BITSET_H
