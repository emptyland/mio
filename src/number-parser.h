#ifndef MIO_NUMBER_PARSER_H_
#define MIO_NUMBER_PARSER_H_

#include "base.h"

namespace mio {

class NumberParser {
public:

    static mio_i8_t ParseDecimalI8(const char *z, size_t n, bool *ok);
    static mio_i16_t ParseDecimalI16(const char *z, size_t n, bool *ok);
    static mio_i32_t ParseDecimalI32(const char *z, size_t n, bool *ok);
    static mio_int_t ParseDecimalInt(const char *z, size_t n, bool *ok);
    static mio_i64_t ParseDecimalI64(const char *z, size_t n, bool *ok);

    static mio_i8_t ParseHexadecimalI8(const char *z, size_t n, bool *ok);
    static mio_i16_t ParseHexadecimalI16(const char *z, size_t n, bool *ok);
    static mio_i32_t ParseHexadecimalI32(const char *z, size_t n, bool *ok);
    static mio_i64_t ParseHexadecimalI64(const char *z, size_t n, bool *ok);

    static mio_f32_t ParseF32(const char *z);
    static mio_f64_t ParseF64(const char *z);

private:
    NumberParser() = delete;
    ~NumberParser() = delete;
}; // class NumberParser

} // namespace mio

#endif // MIO_NUMBER_PARSER_H_
