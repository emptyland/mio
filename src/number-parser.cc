#include "number-parser.h"
#include "glog/logging.h"

namespace mio {

namespace {

template<class T>
struct IntegralTrait {
    enum Constants: int64_t {
        MAX_DEC_LEN = 0,
        MAX_HEX_LEN = 0,
        UMAX = 0,
        MAX = 0,
        MIN = -1,
    };
};

template<>
struct IntegralTrait<mio_i8_t> {
    enum Constants: int64_t {
        MAX_DEC_LEN = sizeof("127") - 1,
        MAX_HEX_LEN = sizeof("ff") - 1,
        UMAX        = 0xff,
        MAX         = INT8_MAX,
        MIN         = INT8_MIN,
    };
};

template<>
struct IntegralTrait<mio_i16_t> {
    enum Constants: int64_t {
        MAX_DEC_LEN = sizeof("32767") - 1,
        MAX_HEX_LEN = sizeof("ffff") - 1,
        UMAX        = 0xffff,
        MAX         = INT16_MAX,
        MIN         = INT16_MIN,
    };
};

template<>
struct IntegralTrait<mio_i32_t> {
    enum Constants: int64_t {
        MAX_DEC_LEN = sizeof("2147483647") - 1,
        MAX_HEX_LEN = sizeof("ffffffff") - 1,
        UMAX        = 0xffffffff,
        MAX         = INT32_MAX,
        MIN         = INT32_MIN,
    };
};

template<>
struct IntegralTrait<mio_i64_t> {
    enum Constants: int64_t {
        MAX_DEC_LEN = sizeof("9223372036854775807") - 1,
        MAX_HEX_LEN = sizeof("ffffffffffffffff") - 1,
        UMAX        = -1,
        MAX         = INT64_MAX,
        MIN         = INT64_MIN,
    };
};

int64_t ParseDecimalUnsignedInteger(const char *z, size_t n, bool *ok) {
    int64_t rv = 0, pow = 1;
    size_t i = n;
    while (i--) {
        if (z[i] < '0' || z[i] > '9') {
            *ok = false;
            return 0;
        }
        rv += (z[i] - '0') * pow;
        pow *= 10;
    }
    return rv;
}

int64_t ParseHexadecimalUnsignedInteger(const char *z, size_t n, bool *ok) {
    int64_t rv = 0, shift = 0;
    size_t i = n;
    while (i--) {
        int64_t bits4 = 0;
        if (z[i] >= '0' && z[i] <= '9') {
            bits4 = z[i] - '0';
        } else if (z[i] >= 'a' && z[i] <= 'f') {
            bits4 = 10 + z[i] - 'a';
        } else if (z[i] >= 'A' && z[i] <= 'F') {
            bits4 = 10 + z[i] - 'A';
        } else {
            *ok = false;
            return 0;
        }

        rv |= (bits4 << shift);
        shift += 4;
    }

    return rv;
}

template<class T>
T DoParseDecimalIntegral(const char *z, size_t n, bool *ok) {
    int sign = 1;
    if (z[0] == '-') {
        sign = -1;
        if (n < 2) {
            *ok = false;
            return 0;
        }
        z++; n--;

        if (n > IntegralTrait<T>::MAX_DEC_LEN) {
            *ok = false;
            return 0;
        }
    }

    auto rv = ParseDecimalUnsignedInteger(z, n, ok);
    if (!*ok) {
        return 0;
    }

    if (sign < 0) {
        rv = -rv;
    }
    if (rv > IntegralTrait<T>::MAX || rv < IntegralTrait<T>::MIN) {
        *ok = false;
        return 0;
    }

    *ok = true;
    return static_cast<T>(rv);
}

template<class T>
T DoParseHexadecimalIntegral(const char *z, size_t n, bool *ok) {
    if (n == 0 || n > IntegralTrait<T>::MAX_HEX_LEN) {
        *ok = false;
        return 0;
    }

    auto rv = ParseHexadecimalUnsignedInteger(z, n, ok);
    if (!*ok) {
        return 0;
    }
    *ok = true;
    return static_cast<T>(rv);
}

} // namespace

/*static*/
mio_i8_t NumberParser::ParseDecimalI8(const char *z, size_t n, bool *ok) {
    DCHECK_NOTNULL(z);
    DCHECK_NOTNULL(ok);

    return DoParseDecimalIntegral<mio_i8_t>(z, n, ok);
}

/*static*/
mio_i16_t NumberParser::ParseDecimalI16(const char *z, size_t n, bool *ok) {
    DCHECK_NOTNULL(z);
    DCHECK_NOTNULL(ok);

    return DoParseDecimalIntegral<mio_i16_t>(z, n, ok);
}

/*static*/
mio_i32_t NumberParser::ParseDecimalI32(const char *z, size_t n, bool *ok) {
    DCHECK_NOTNULL(z);
    DCHECK_NOTNULL(ok);

    return DoParseDecimalIntegral<mio_i32_t>(z, n, ok);
}

/*static*/
mio_int_t NumberParser::ParseDecimalInt(const char *z, size_t n, bool *ok) {
    DCHECK_NOTNULL(z);
    DCHECK_NOTNULL(ok);

    return DoParseDecimalIntegral<mio_i64_t>(z, n, ok);
}

/*static*/
mio_i64_t NumberParser::ParseDecimalI64(const char *z, size_t n, bool *ok) {
    DCHECK_NOTNULL(z);
    DCHECK_NOTNULL(ok);

    return DoParseDecimalIntegral<mio_i64_t>(z, n, ok);
}

/*static*/
mio_f32_t NumberParser::ParseF32(const char *z) {
    DCHECK_NOTNULL(z);
    return atof(z);
}

/*static*/
mio_f64_t NumberParser::ParseF64(const char *z) {
    DCHECK_NOTNULL(z);
    return atof(z);
}

/*static*/
mio_i8_t NumberParser::ParseHexadecimalI8(const char *z, size_t n, bool *ok) {
    DCHECK_NOTNULL(z);
    DCHECK_NOTNULL(ok);

    return DoParseHexadecimalIntegral<mio_i8_t>(z, n, ok);
}

/*static*/
mio_i16_t NumberParser::ParseHexadecimalI16(const char *z, size_t n, bool *ok) {
    DCHECK_NOTNULL(z);
    DCHECK_NOTNULL(ok);

    return DoParseHexadecimalIntegral<mio_i16_t>(z, n, ok);
}

/*static*/
mio_i32_t NumberParser::ParseHexadecimalI32(const char *z, size_t n, bool *ok) {
    DCHECK_NOTNULL(z);
    DCHECK_NOTNULL(ok);

    return DoParseHexadecimalIntegral<mio_i32_t>(z, n, ok);
}

/*static*/
mio_i64_t NumberParser::ParseHexadecimalI64(const char *z, size_t n, bool *ok) {
    DCHECK_NOTNULL(z);
    DCHECK_NOTNULL(ok);

    return DoParseHexadecimalIntegral<mio_i64_t>(z, n, ok);
}

} // namespace mio
