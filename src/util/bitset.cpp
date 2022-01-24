//
// Created by "Simon Yu" on 2022/1/14.
//

#include "src/util/BitSet.h"

namespace util {

BitSet::BitSet(size_type N)
    : inner_blocks({0}), m_size((N + block_bits - 1) / block_bits), nbits(N) {
    allocMemory(m_size, true);
}

BitSet::BitSet(BitSet const &other) {
    *this = other;
    // 1. Do not release memory in `m_data`, it belongs to `other`.
    // 2. Now this->m_size == other.m_size.
    allocMemory(m_size, false);
    for (size_type i = 0; i < m_size; ++i) {
        m_data[i] = other.m_data[i];
    }
}

BitSet::BitSet(BitSet &&other) noexcept {
    *this = other;
    other.m_data = nullptr;
}

// Containers have constructed dirty objects for us ;-(
// Initializing all data in class definition is a solution.
// Or, just change size every time copy assignment happens, and 
// forget about memory reuse even if you have enough capacity.
BitSet &BitSet::operator=(BitSet const &other) {
    // Self-assignment check is important
    if (this == &other) return *this;

    freeMemory();
    allocMemory(other.m_size, true);

    nbits = other.nbits;
    m_size = other.m_size;
    inner_blocks = other.inner_blocks;
    for (size_type i = 0; i < other.m_size; ++i) {
        m_data[i] = other.m_data[i];
    }
    return *this;
}

// This operator is not even used...
BitSet &BitSet::operator=(BitSet &&other) {
    if (this == &other) return *this;

    freeMemory();
    *this = other;
    other.m_data = nullptr;

    return *this;
}

void BitSet::allocMemory(size_type size, bool setBitsToZeros) {
    m_data = (size > n_inner_blocks) ? new block_type[size] : &inner_blocks[0];
    if (!setBitsToZeros) return;
    for (size_type i = 0; i < size; ++i) {
        m_data[i] = 0;
    }
}

void BitSet::freeMemory() {
    if (m_size > n_inner_blocks) {
        delete[] m_data;
        m_data = nullptr;
    }
}

bool BitSet::hasIntersection(BitSet const &other) const {
    for (size_type i = 0; i < m_size; ++i) {
        if (m_data[i] & other.m_data[i]) return true;
    }
    return false;
}

BitSet &BitSet::operator&=(BitSet const &other) {
    for (size_type i = 0; i < m_size; ++i) {
        m_data[i] &= other.m_data[i];
    }
    return *this;
}

BitSet &BitSet::operator|=(BitSet const &other) {
    for (size_type i = 0; i < m_size; ++i) {
        m_data[i] |= other.m_data[i];
    }
    return *this;
}

BitSet &BitSet::operator^=(BitSet const &other) {
    for (size_type i = 0; i < m_size; ++i) {
        m_data[i] ^= other.m_data[i];
    }
    return *this;
}

BitSet::~BitSet() { freeMemory(); }

void BitSet::set(size_type N, bool flag) {
    if (N >= nbits) {
        throw std::runtime_error(
            "BitSet(): set(): Given index is out of range");
    }
    if (flag)
        m_data[N / block_bits] |= 1LLU << (N % block_bits);
    else
        m_data[N / block_bits] &= ~(1LLU << (N % block_bits));
}

bool BitSet::contains(size_type N) const {
    if (N >= nbits) {
        throw std::runtime_error(
            "BitSet(): contains(): Given index is out of range");
    }
    return m_data[N / block_bits] & (1LLU << (N % block_bits));
}

void BitSet::clear() {
    if (!m_data) {
        allocMemory(false);
    }
    for (size_type i = 0; i < m_size; ++i) m_data[i] = 0;
}

bool BitSet::operator==(BitSet const &other) const {
    BitSet const *smaller = this;
    BitSet const *larger = &other;
    if (smaller->m_size > larger->m_size) {
        std::swap(smaller, larger);
    }
    size_type sz = smaller->m_size;
    for (size_type i = 0; i < sz; ++i) {
        if (m_data[i] != other.m_data[i]) return false;
    }
    for (size_type i = sz; i < larger->m_size; ++i) {
        if (larger->m_data[i]) return false;
    }
    return true;
}

BitSet::Iterator::Iterator(const BitSet &_bitset, bool end)
    : bitset(_bitset),
      mask(mask_all_ones),
      index(end ? npos : 0),
      availablePos(npos) {
    if (!end) advance();
}

auto BitSet::Iterator::operator++() -> Iterator & {
    advance();
    return *this;
}

bool BitSet::Iterator::advance() {
    if (index == npos) return false;

    block_type x = 0;
    while (index < bitset.m_size && !(x = bitset.m_data[index] & mask)) {
        ++index;
        mask = mask_all_ones;
    }

    if (!x) {
        availablePos = npos;
        index = npos;
        mask = mask_all_ones;
        return false;
    }

    x = x & (block_type)(-(block_signed_type)x);  // Get right most bit
    mask &= ~x;                                   // Set mask for further use

    size_type pos = block_bits - 1;
    if (x & 0x0000'ffff) pos -= 16;
    if (x & 0x00ff'00ff) pos -= 8;
    if (x & 0x0f0f'0f0f) pos -= 4;
    if (x & 0x3333'3333) pos -= 2;
    if (x & 0x5555'5555) --pos;

    // Add offset to global position
    pos = index * block_bits + pos;

    availablePos = pos;
    return true;
}

BitSet::Iterator::value_type BitSet::Iterator::operator*() const {
    return availablePos;
}

bool operator!=(const BitSet::Iterator &a, const BitSet::Iterator &b) {
    return &a.bitset != &b.bitset || a.index != b.index || a.mask != b.mask;
}

void BitSet::add(BitSet::size_type N) { set(N, true); }

void BitSet::remove(BitSet::size_type N) { set(N, false); }

String BitSet::dump() const {
    String s = "[";
    bool flag = false;
    for (auto i : *this) {
        if (flag) s += ", ";
        s += std::to_string(i);
        flag = true;
    }
    s += ']';
    return s;
}

}  // namespace util