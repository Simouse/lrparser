#ifndef __THIN_STRING__
#define __THIN_STRING__

namespace util {

using size32_t = unsigned int;
using size8_t = unsigned char;
using size_t = unsigned long long;

class Str {
    static constexpr size32_t inplaceCapacity = 15;
    static constexpr size32_t maxPossibleCapacity = 0x7fffffff;
    static inline constexpr size_t max(size_t a, size_t b) {
        return a > b ? a : b;
    }

    union {
        struct {
            bool inplace : 1;
            size8_t isize : 7;
            char idata[15];
        };
        struct {
            size32_t unused : 1;
            size32_t dcapacity : 31;
            size32_t dsize;
            char *dpointer;
        };
    };

    char *data() noexcept;
    void setSize(size32_t n);
    // Unsafe if len is not right
    void append(const char *src, size_t len);
    // Make string empty but do not reclaim memory
    void reset();

  public:
    // All following constructors should call this one first
    Str() : inplace(true), isize(0) { idata[0] = '\0'; }
    Str(const char *s, size32_t len);
    Str(const char *s);
    Str(const Str &s);
    Str(Str &&s) noexcept;
    ~Str();
    [[nodiscard]] const char *c_str() const noexcept;
    [[nodiscard]] size32_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    // Include '\0'
    [[nodiscard]] size32_t capacity() const noexcept;
    void append(const char *s);
    bool reserve(size_t newCap);
    Str &operator=(const char *s);
    Str &operator=(const Str &s);
    Str &operator+=(char ch);
    Str &operator+=(const char *s);
    Str &operator+=(const Str &s);
    const char &operator[](int index) const;
    char &operator[](int index);
};

} // namespace util

#endif