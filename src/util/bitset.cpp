//
// Created by "Simon Yu" on 2022/1/14.
//

#include "src/util/BitSet.h"

#include <cassert>
#include <type_traits>

namespace util {

BitSet::BitSet(size_type N)
    : inner_blocks({0}), m_size((N + block_bits - 1) / block_bits) {
    // Make sure m_size is at least n_inner_blocks
    if (m_size < n_inner_blocks) m_size = n_inner_blocks;
    // Initialize m_data
    allocMemory(m_size, true);
}

BitSet::BitSet(BitSet const &other) {
    copyContent(other);
    // 1. Do not release memory in `m_data`, it belongs to `other`.
    // 2. Now this->m_size == other.m_size.
    allocMemory(m_size, false);
    copyRange(m_data, other.m_data, 0, m_size);
}

BitSet::BitSet(BitSet &&other) noexcept {
    copyContent(other);
    other.m_data = nullptr;
}

// Caveat: small bitsets have self-references. So the moved `m_data`
// cannot be used directly. We have to check if it must be modified
// to point to `this->&inner_blocks[0]`
void BitSet::copyContent(BitSet const &other) {
    using ClassType = std::remove_reference<decltype(*this)>::type;
    using CastType = std::array<unsigned char, sizeof(ClassType)>;
    *(CastType *)this = *(CastType *)&other;
    // this->m_data = other.m_data;
    // this->m_size = other.m_size;
    // this->inner_blocks = other.inner_blocks;

    if (other.m_data == &other.inner_blocks[0]) {
        this->m_data = &inner_blocks[0];
    }
}

// Containers have constructed dirty objects for us ;-(
// 2022/1/25: This is caused by usages of operator= in constructors,
// data is not initalized completely by that time.
BitSet &BitSet::operator=(BitSet const &other) {
    // Self-assignment check
    if (this == &other) return *this;

    freeMemory();
    copyContent(other);
    allocMemory(other.m_size, true);
    copyRange(m_data, other.m_data, 0, other.m_size);
    return *this;
}

// This operator is not even used...
BitSet &BitSet::operator=(BitSet &&other) noexcept {
    if (this == &other) return *this;

    freeMemory();
    copyContent(other);
    other.m_data = nullptr;

    return *this;
}

void BitSet::allocMemory(size_type size, bool setBitsToZeros) {
    m_data = (size > n_inner_blocks) ? new block_type[size] : &inner_blocks[0];
    m_size = size;
    if (!setBitsToZeros) return;
    fillZeros(m_data, 0, size);
}

void BitSet::freeMemory() {
    if (m_data && m_data != &inner_blocks[0]) {
        assert(m_size > n_inner_blocks);

        delete[] m_data;
        m_data = nullptr;
    }
}

bool BitSet::hasIntersection(BitSet const &other) const {
    auto min_size = m_size < other.m_size ? m_size : other.m_size;
    for (size_type i = 0; i < min_size; ++i) {
        if (m_data[i] & other.m_data[i]) return true;
    }
    return false;
}

BitSet &BitSet::operator&=(BitSet const &other) {
    ensure(other.m_size * block_bits);
    for (size_type i = 0; i < m_size; ++i) {
        m_data[i] &= other.m_data[i];
    }
    return *this;
}

BitSet &BitSet::operator|=(BitSet const &other) {
    ensure(other.m_size * block_bits);
    for (size_type i = 0; i < m_size; ++i) {
        m_data[i] |= other.m_data[i];
    }
    return *this;
}

BitSet &BitSet::operator^=(BitSet const &other) {
    ensure(other.m_size * block_bits);
    for (size_type i = 0; i < m_size; ++i) {
        m_data[i] ^= other.m_data[i];
    }
    return *this;
}

BitSet::~BitSet() { freeMemory(); }

void BitSet::set(size_type N, bool flag) {
    assert(N < m_size * block_bits);
    if (flag)
        m_data[N / block_bits] |= 1LLU << (N % block_bits);
    else
        m_data[N / block_bits] &= ~(1LLU << (N % block_bits));
}

bool BitSet::contains(size_type N) const {
    if (N >= m_size * block_bits) return false;
    return m_data[N / block_bits] & (1LLU << (N % block_bits));
}

void BitSet::clear() {
    // If bitset data is corrupted (moved), allocate new mmeory for it
    if (!m_data) {
        allocMemory(m_size, false);
    }
    // Clear memory
    for (size_type i = 0; i < m_size; ++i) m_data[i] = 0;
}

void BitSet::fillZeros(block_type *data, size_type from, size_type to) {
    for (size_type i = from; i < to; ++i) data[i] = 0;
}

void BitSet::copyRange(block_type *dest, block_type *src, size_type from,
                       size_type to) {
    for (size_type i = from; i < to; ++i) dest[i] = src[i];
}

void BitSet::ensure(size_type N) {
    size_type leastBlocksNeeded = ((N + block_bits - 1) / block_bits);
    if (leastBlocksNeeded <= m_size) {
        return;
    }
    auto prevData = m_data;
    auto prevSize = m_size;
    size_type capacity = leastBlocksNeeded | (m_size << 1);
    // Because m_size is always larger than or equal to n_inner_blocks, and
    // leastBlocksNeeded > m_size now, we know that inner blocks cannot be used.
    // allocMemory() will definitely request a dynamically allocated memory
    // space.
    allocMemory(capacity, false);
    copyRange(m_data, prevData, 0, prevSize);
    fillZeros(m_data, prevSize, m_size);
    if (prevData != &inner_blocks[0]) {
        delete[] prevData;
    }
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
    String s = "{";
    bool flag = false;
    for (auto i : *this) {
        if (flag) s += ", ";
        s += std::to_string(i);
        flag = true;
    }
    s += '}';
    return s;
}

}  // namespace util