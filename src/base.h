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

} // namespace mio

#endif // MIO_BASE_H_
