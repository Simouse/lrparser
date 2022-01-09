#include "util/str.h"
#include <cstring>
#include <stdexcept>

using util::size32_t;

namespace util {

char *Str::data() noexcept { return inplace ? &idata[0] : dpointer; }

void Str::setSize(size32_t n) {
  if (inplace)
    isize = n;
  else
    dsize = n;
}

// Unsafe if len is not right
void Str::append(const char *src, size_t len) {
  size_t sz = size();
  size_t needCap = sz + len + 1;

  if (needCap > capacity() && !reserve(max(sz * 2, needCap))) {
    throw std::runtime_error("Size exceeds max designed capacity");
  }

  char *dst = data();
  std::strncpy(dst + sz, src, len);
  dst[sz + len] = '\0';
  setSize(sz + len);
}

// Make string empty but do not reclaim memory
void Str::reset() {
  setSize(0);
  char *p = data();
  if (p != nullptr) {
    p[0] = '\0';
  }
}

Str::Str(const char *s, size32_t len) : Str() { append(s, len); }
Str::Str(const char *s) : Str() { append(s); }
Str::Str(const Str &s) : Str() { append(s.c_str(), s.size()); }
Str::Str(Str &&s) noexcept : Str() {
  memcpy((void *)this, (void *)&s, sizeof(Str));
  s.dpointer = nullptr;
  s.inplace = true;
  s.isize = 0;
}

// C-style string, or const string
[[nodiscard]] const char *Str::c_str() const noexcept {
  return inplace ? &idata[0] : dpointer;
}

[[nodiscard]] size32_t Str::size() const noexcept {
  return inplace ? isize : dsize;
}

[[nodiscard]] bool Str::empty() const noexcept { return size() == 0; }

// Include '\0'
[[nodiscard]] size32_t Str::capacity() const noexcept {
  return inplace ? inplaceCapacity : dcapacity;
}

void Str::append(const char *s) {
  size_t len = std::strlen(s);
  append(s, len);
}

bool Str::reserve(size_t newCap) {
  if (newCap > maxPossibleCapacity)
    return false;
  if (newCap <= capacity())
    return true;

  size_t sz = size();
  char *newp = new char[newCap];
  std::memcpy(newp, c_str(), sz);
  newp[sz + 1] = '\0';

  if (!inplace)
    delete[] dpointer;

  dpointer = newp;
  dcapacity = newCap;
  inplace = false;
  return true;
}

Str &Str::operator+=(char ch) {
  size32_t sz = size();

  if (sz + 1 >= capacity() && !reserve((size_t)sz * 2) && !reserve(sz + 2)) {
    throw std::runtime_error("Size exceeds max designed capacity");
  }

  char *s = data();
  s[sz] = ch;
  s[sz + 1] = '\0';
  setSize(sz + 1);
  return *this;
}

Str &Str::operator=(const char *s) {
  reset();
  append(s);
  return *this;
}

Str &Str::operator=(const Str &s) {
  reset();
  append(s.c_str(), s.size());
  return *this;
}

Str &Str::operator+=(const char *s) {
  append(s);
  return *this;
}

Str &Str::operator+=(const Str &s) {
  append(s.c_str(), s.size());
  return *this;
}

const char &Str::operator[](int index) const { return c_str()[index]; }
char &Str::operator[](int index) { return data()[index]; }

Str::~Str() {
  if (!inplace) {
    delete[] dpointer;
    dpointer = nullptr;
    inplace = true;
    idata[0] = '\0';
  }
}

} // namespace util