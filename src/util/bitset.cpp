//
// Created by "Simon Yu" on 2022/1/14.
//

#include "src/util/BitSet.h"

namespace util {

BitSet::BitSet(size_type N)
    : nbits(N), size((N + block_bits - 1) / block_bits) {
  data = new block_type[size]{0};
}

BitSet::BitSet(BitSet const &other) : size(other.size), nbits(other.nbits) {
  data = new block_type[size];
  for (size_type i = 0; i < size; ++i) {
    data[i] = other.data[i];
  }
}

BitSet::BitSet(BitSet &&other) noexcept
    : size(other.size), nbits(other.nbits), data(other.data) {
  other.data = nullptr;
}

bool BitSet::hasIntersection(BitSet const &other) const {
  for (size_type i = 0; i < size; ++i) {
    if (data[i] & other.data[i]) return true;
  }
  return false;
}

BitSet &BitSet::operator=(BitSet const &other) {
  // Self-assignment check is important
  if (this == &other) return *this;

  size = other.size;
  nbits = other.nbits;
  delete[] data;
  data = new block_type[size];
  for (size_type i = 0; i < size; ++i) {
    data[i] = other.data[i];
  }
  return *this;
}

BitSet &BitSet::operator&=(BitSet const &other) {
  for (size_type i = 0; i < size; ++i) {
    data[i] &= other.data[i];
  }
  return *this;
}
BitSet &BitSet::operator|=(BitSet const &other) {
  for (size_type i = 0; i < size; ++i) {
    data[i] |= other.data[i];
  }
  return *this;
}
BitSet &BitSet::operator^=(BitSet const &other) {
  for (size_type i = 0; i < size; ++i) {
    data[i] ^= other.data[i];
  }
  return *this;
}

BitSet::~BitSet() { delete[] data; }

void BitSet::set(size_type N, bool flag) {
  if (N >= nbits) {
    throw std::runtime_error("BitSet(): set(): Given index is out of range");
  }
  //    if (flag)
  //        data[N / block_bits] |= block_start >> (N % block_bits);
  //    else
  //        data[N / block_bits] &= ~(block_start >> (N % block_bits));
  if (flag)
    data[N / block_bits] |= 1LLU << (N % block_bits);
  else
    data[N / block_bits] &= ~(1LLU << (N % block_bits));
}

bool BitSet::contains(size_type N) const {
  if (N >= nbits) {
    throw std::runtime_error(
        "BitSet(): contains(): Given index is out of range");
  }
  //    return data[N / block_bits] & (block_start >> (N % block_bits));
  return data[N / block_bits] & (1LLU << (N % block_bits));
}

void BitSet::clear() {
  if (!data) {
    data = new block_type[size];
  }
  for (size_type i = 0; i < size; ++i) data[i] = 0;
}

bool BitSet::operator==(BitSet const &other) const {
  BitSet const *smaller = this;
  BitSet const *larger = &other;
  if (smaller->size > larger->size) {
    std::swap(smaller, larger);
  }
  size_type sz = smaller->size;
  for (size_type i = 0; i < sz; ++i) {
    if (data[i] != other.data[i]) return false;
  }
  for (size_type i = sz; i < larger->size; ++i) {
    if (larger->data[i]) return false;
  }
  return true;
}

BitSet::Iterator::Iterator(const BitSet &_bitset, bool end)
    : bitset(_bitset),
      index(end ? npos : 0),
      mask(mask_all_ones),
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
  while (index < bitset.size && !(x = bitset.data[index] & mask)) {
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