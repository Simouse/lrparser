#ifndef HASH_SET
#define HASH_SET

#include <algorithm>
#include <type_traits>
#include <unordered_set>


namespace util {

// BitSet-compatible adaptation for std::unordered_set.

template <class T> class HashSet : public std::unordered_set<int> {
    using element_type = int;
    using size_type = size_t;

  public:
    static_assert(std::is_integral_v<T> || std::is_enum_v<T>,
                  "HashSet can only hold unsigned types");

    HashSet() = default;

    explicit HashSet(size_type N) {}

    void remove(T N_) { std::unordered_set<element_type>::erase(N_); }

    void insert(T N_) { std::unordered_set<element_type>::insert(N_); }

    bool contains(T N_) const {
        return std::unordered_set<element_type>::count(N_) != 0;
    }

    HashSet &operator|=(HashSet const &other) {
        for (auto e : other) {
            this->insert(e);
        }
        return *this;
    }

    HashSet &operator&=(HashSet const &other) {
        HashSet result;
        for (auto e : other) {
            if (this->contains(e))
                result.insert(e);
        }
        this->swap(result);
        return *this;
    }
};

} // namespace util

namespace std {
template <class T> struct hash<util::HashSet<T>> {
    size_t operator()(util::HashSet<T> const &s) const {
        size_t result = 17;
        for (auto e : s) {
            result = result * 31 + e;
        }
        return result;
    }
};
} // namespace std

#endif