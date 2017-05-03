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
    M(EOF, 0, "") \
    M(QUESTION, 0, "?") \
    M(EXCLAMATION, 0, "!") \
    M(LPAREN, 0, "(") \
    M(RPAREN, 0, ")") \
    M(LBRACK, 0, "[") \
    M(RBRACK, 0, "]") \
    M(LBRACE, 0, "{") \
    M(RBRACE, 0, "}") \
    M(COMMA, 0, ",") \
    M(LINE_COMMENT, 0, "# ...\\n") \
    M(DOT, 0, ".") \
    M(TWO_DOT, 0, "..") \
    M(COLON, 0, ":") \
    M(NAME_BREAK, 0, "::") \
    M(THIN_RARROW, 0, "->") \
    M(THIN_LARROW, 0, "<-") \
    M(ASSIGN, 0, "=") \
    M(PLUS, 0, "+") \
    M(STAR, 0, "*") \
    M(PERCENT, 0, "%") \
    M(SLASH, 0, "/") \
    M(MINUS, 0, "-") \
    M(LSHIFT, 0, "<<") \
    M(RSHIFT_L, 0, "|>") \
    M(RSHIFT_A, 0, ">>") \
    M(BIT_OR, 0, "|") \
    M(BIT_AND, 0, "&") \
    M(BIT_XOR, 0, "^") \
    M(WAVE, 0, "~") \
    M(EQ, 0, "==") \
    M(NE, 0, "<>") \
    M(LE, 0, "<=") \
    M(LT, 0, "<") \
    M(GE, 0, ">=") \
    M(GT, 0, ">") \
    M(AND, 0, "and") \
    M(OR, 0, "or" ) \
    M(NOT, 0, "not") \
    M(PACKAGE, 0, "package") \
    M(WITH, 0, "with") \
    M(AS, 0, "as") \
    M(IS, 0, "is") \
    M(BOOL, 0, "bool") \
    M(I8, 0, "i8") \
    M(I16, 0, "i16") \
    M(I32, 0, "i32") \
    M(INT, 0, "int") \
    M(I64, 0, "i64") \
    M(F32, 0, "f32") \
    M(F64, 0, "f64") \
    M(STRING, 0, "string") \
    M(VOID, 0, "void") \
    M(UNION, 0, "union") \
    M(MAP, 0, "map") \
    M(SLICE, 0, "slice") \
    M(ARRAY, 0, "array") \
    M(STRUCT, 0, "struct") \
    M(ERROR_TYPE, 0, "error") \
    M(WEAK, 0, "weak") \
    M(STRONG, 0, "strong") \
    M(ID, 0, "[$_a-zA-Z0-9]+") \
    M(I8_LITERAL, 0, "\\d+b") \
    M(I16_LITERAL, 0, "\\d+w") \
    M(I32_LITERAL, 0, "\\d+d") \
    M(INT_LITERAL, 0, "\\d+") \
    M(I64_LITERAL, 0, "\\d+q") \
    M(F32_LITERAL, 0, "\\d*\\.\\d+F") \
    M(F64_LITERAL, 0, "\\d*\\.\\d+D") \
    M(STRING_LITERAL, 0, "\'...\'") \
    M(IF, 0, "if") \
    M(ELSE, 0, "else") \
    M(WHILE, 0, "while") \
    M(FOR, 0, "for") \
    M(MATCH, 0, "match") \
    M(IN, 0, "in") \
    M(RETURN, 0, "return") \
    M(BREAK, 0, "break") \
    M(CONTINUE, 0, "continue") \
    M(VAL, 0, "val") \
    M(VAR, 0, "var") \
    M(FUNCTION, 0, "function") \
    M(LAMBDA, 0, "lambda") \
    M(NATIVE, 0, "native") \
    M(EXPORT, 0, "export") \
    M(DEF, 0, "def") \
    M(TRUE, 0, "true") \
    M(FALSE, 0, "false")


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
