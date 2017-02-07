#ifndef MIO_TOKEN_INL_H_
#define MIO_TOKEN_INL_H_

#if defined(__cplusplus)
namespace mio {
#endif

//  primitive
#define DEFINE_PRIMITIVE_TYPES(M) \
    M(bool,   mio_bool_t,   1, "" )  \
    M(i8,     mio_i8_t,     1, "b")  \
    M(i16,    mio_i16_t,    2, "w")  \
    M(i32,    mio_i32_t,    4, "d")  \
    M(int,    mio_int_t,    8, "" )  \
    M(i64,    mio_i64_t,    8, "q")  \
    M(f32,    mio_f32_t,    4, "F")  \
    M(f64,    mio_f64_t,    8, "D")

// All tokens
#define DEFINE_TOKENS(M) \
    M(ERROR, 0, "") \
    M(EOF, 0,   "") \
    M(LINE_COMMENT, 0, "# ...\\n") \
    M(ASSIGN, 0, "=") \
    M(MINUS, 0, "-") \
    M(EQ, 0,     "==") \
    M(I8_LITERAL, 0, "\\d+b") \
    M(I16_LITERAL, 0, "\\d+w") \
    M(I32_LITERAL, 0, "\\d+d") \
    M(INT_LITERAL, 0, "\\d+") \
    M(I64_LITERAL, 0, "\\d+q") \
    M(F32_LITERAL, 0, "\\d*\\.\\d+f") \
    M(F64_LITERAL, 0, "\\d*\\.\\d+d")


#define Token_CODE_ENUM(name, prio, text) TOKEN_##name,

#if defined(__cplusplus)
enum Token: int {
    DEFINE_TOKENS(Token_CODE_ENUM)
};
#else
enum Token {
    DEFINE_TOKENS(Token_CODE_ENUM)
};
#endif

#undef Token_CODE_ENUM

#if defined(__cplusplus)
}
#endif

#endif // MIO_TOKEN_INL_H_
