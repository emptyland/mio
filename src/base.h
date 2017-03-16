#ifndef MIO_BASE_H_
#define MIO_BASE_H_

#include <stddef.h>
#include <stdint.h>

namespace mio {

/**
 * The mio types:
 * 
 * mio_i8_t  -> fixed  8 bits
 * mio_i16_t -> fixed 16 bits
 * mio_i32_t -> fixed 32 bits
 * mio_int_t -> fixed 64 bits
 * mio_i64_t -> fixed 64 bits
 * mio_float_t -> fixed 32 bits floating number
 * mio_double_t -> fixed 64 bits floating number
 */

typedef int8_t   mio_bool_t;
typedef int8_t   mio_i8_t;
typedef int16_t  mio_i16_t;
typedef int32_t  mio_i32_t;
typedef int64_t  mio_i64_t;
typedef int64_t  mio_int_t;
typedef float    mio_f32_t;
typedef double   mio_f64_t;

#define DISALLOW_IMPLICIT_CONSTRUCTORS(clazz_name) \
    clazz_name (const clazz_name &) = delete;      \
    clazz_name (clazz_name &&) = delete;           \
    void operator = (const clazz_name &) = delete;

/**
 * Constants
 *
 */
static const int kPageSizeShift = 12;

static const int kPageSize = (1 << kPageSizeShift); // 4 KB

static const int kAlignmentSize = 4;

/**
 * disable copy constructor, assign operator function.
 *
 */
class DisallowImplicitConstructors {
public:
    DisallowImplicitConstructors() = default;

    DISALLOW_IMPLICIT_CONSTRUCTORS(DisallowImplicitConstructors)
};

/**
 * Get size of array.
 */
template <class T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

#ifndef _MSC_VER
template <class T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];
#endif

#define arraysize(array) (sizeof(::mio::ArraySizeHelper(array)))

/**
 * define getter/mutable_getter/setter
 */
#define DEF_PROP_RMW(type, name) \
    DEF_GETTER(type, name) \
    DEF_MUTABLE_GETTER(type, name) \
    DEF_SETTER(type, name)

#define DEF_PROP_RW(type, name) \
    DEF_GETTER(type, name) \
    DEF_SETTER(type, name)

#define DEF_GETTER(type, name) \
    inline const type &name() const { return name##_; }

#define DEF_MUTABLE_GETTER(type, name) \
    inline type *mutable_##name() { return &name##_; }

#define DEF_SETTER(type, name) \
    inline void set_##name(const type &value) { name##_ = value; }

template<class T, class F>
inline T *down_cast(F *from) {
#ifndef NDEBUG
    DCHECK_NOTNULL(dynamic_cast<T*>(from));
#endif
    return static_cast<T*>(from);
}

#define IS_POWER_OF_TWO(x) (((x) & ((x) - 1)) == 0)

// Returns true iff x is a power of 2 (or zero). Cannot be used with the
// maximally negative value of the type T (the -1 overflows).
template <typename T>
inline bool IsPowerOf2(T x) {
    return IS_POWER_OF_TWO(x);
}

// Compute the 0-relative offset of some absolute value x of type T.
// This allows conversion of Addresses and integral types into
// 0-relative int offsets.
template <typename T>
inline intptr_t OffsetFrom(T x) {
    return x - static_cast<T>(0);
}


// Compute the absolute value of type T for some 0-relative offset x.
// This allows conversion of 0-relative int offsets into Addresses and
// integral types.
template <typename T>
inline T AddressFrom(intptr_t x) {
    return static_cast<T>(static_cast<T>(0) + x);
}


// Return the largest multiple of m which is <= x.
template <typename T>
inline T RoundDown(T x, intptr_t m) {
    //DCHECK(IsPowerOf2(m));
    return AddressFrom<T>(OffsetFrom(x) & -m);
}


// Return the smallest multiple of m which is >= x.
template <typename T>
inline T RoundUp(T x, intptr_t m) {
    return RoundDown<T>(static_cast<T>(x + m - 1), m);
}

inline size_t AlignDownBounds(size_t bounds, size_t value) {
    return (value + bounds - 1) & (~(bounds - 1));
}

// Round bytes filling
// For int16, 32, 64 filling:
void *Round16BytesFill(const uint16_t zag, void *chunk, size_t n);
void *Round32BytesFill(const uint32_t zag, void *chunk, size_t n);
void *Round64BytesFill(const uint64_t zag, void *chunk, size_t n);

} // namespace mio

#endif // MIO_BASE_H_
