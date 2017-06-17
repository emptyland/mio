#ifndef MIO_NYAA_TYPES_H_
#define MIO_NYAA_TYPES_H_

#include <stdint.h>

namespace mio {

#define NYAA_TYPES(M) \
    M(Int8,    i8)    \
    M(Int16,   i16)   \
    M(Int32,   i32)   \
    M(Int64,   i64)   \
    M(Float32, f32)   \
    M(Float64, f64)   \
    M(Object,  object)

class NType {
public:
    enum Kind: int16_t {
    #define DEFINE_ENUM(name, short_name) k##name,
        NYAA_TYPES(DEFINE_ENUM)
    #undef  DEFINE_ENUM
    };

#define DEFINE_CREATOR(name, short_name) \
    static NType name () { return NType(k##name); }
    NYAA_TYPES(DEFINE_CREATOR)
#undef  DEFINE_CREATOR

    Kind kind() const { return kind_; }

private:
    explicit NType(Kind kind) : kind_(kind) {}

    Kind kind_;
};

extern const char * const kNTypeShortNames[];

} // namespace mio

#endif // MIO_NYAA_TYPES_H_
