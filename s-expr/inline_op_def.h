#ifndef MIO_S_EXPR_INLINE_OP_DEF_H_
#define MIO_S_EXPR_INLINE_OP_DEF_H_

#ifdef __cplusplus
namespace mio {
#endif

#define DECL_INLINE_OP(_)        \
    _(LAMBDA, lambda, "lambda")  \
    _(DEF,    def,    "def")     \
    _(LET,    let,    "let")     \
    _(PLUS,   plus,   "+")       \
    _(MINUS,  minus,  "-")       \
    _(MULTI,  multi,  "*")       \
    _(DIVI,   divi,   "/")       \
    _(IF,     branch, "if")      \
    _(EQ,     equals, "=")       \
    _(BEGIN,  begin,  "begin")

enum InlineOp {
#define DEF_INLINE_OP(code, name, op) \
    OP_##code,

DECL_INLINE_OP(DEF_INLINE_OP)

#undef DEF_INLINE_OP
};

#ifndef NO_DEF_MIO_INLINE_OP
struct mio_inline_op {
    const char *z;
    int id;
};
#endif

const struct mio_inline_op *
lex_inline_op (register const char *str, register unsigned int len);

#ifdef __cplusplus
} // namespace mio
#endif

#endif // MIO_S_EXPR_INLINE_OP_DEF_H_
