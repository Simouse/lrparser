#ifndef TREE_SET
#define TREE_SET

#include <algorithm>
#include <functional>
#include <iterator>
#include <set>
#include <type_traits>

namespace util {

// BitSet-compatible adaptation for std::set.

template <class T> class TreeSet : public std::set<int> {
    using element_type = int;
    using size_type = size_t;
  
  public:
    static_assert(std::is_integral_v<T> || std::is_enum_v<T>,
                  "TreeSet can only hold unsigned types");

    TreeSet() = default;

    explicit TreeSet(size_type N) {}

    void remove(T N_) { std::set<element_type>::erase(N_); }

    void insert(T N_) { std::set<element_type>::insert(N_); }

    bool contains(T N_) const { return std::set<element_type>::count(N_) != 0; }

    TreeSet &operator|=(TreeSet const &other) {
        TreeSet result;
        std::set_union(this->begin(), this->end(), other.begin(),
                       other.end(), std::back_inserter(result));
        this->swap(result);
        return *this;
    }

    TreeSet &operator&=(TreeSet const &other) {
        TreeSet result;
        std::set_intersection(this->begin(), this->end(), other.begin(),
                              other.end(), std::back_inserter(result));
        this->swap(result);
        return *this;
    }
};

}

namespace std {
template<class T>
struct hash<util::TreeSet<T>> {
    size_t operator()(util::TreeSet<T> const &s) const {
        size_t result = 17;
        for (auto e : s) {
            result = result * 31 + e;
        }
        return result;
    }
};
}

#endif