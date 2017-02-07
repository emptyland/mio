#ifndef MIO_LEXER_H_
#define MIO_LEXER_H_

#include "base.h"
#include "glog/logging.h"
#include <stdarg.h>

namespace mio {

class TextInputStream;
class TokenObject;

class Lexer {
public:
    struct Scope {
        Scope *top = nullptr;
        TextInputStream *input_stream = nullptr;
        bool ownership = false;
        int ahead = 0;
        int line = 0;
        int column = 0;
        int position = 0;
    };

    Lexer(TextInputStream *input_stream, bool ownership);
    Lexer() = default;
    ~Lexer();

    TextInputStream *input_stream() const {
        return DCHECK_NOTNULL(current_)->input_stream;
    }

    Scope *current() { return DCHECK_NOTNULL(current_); }

    DEF_PROP_RW(bool, dont_ignore_comments)

    void PushScope(TextInputStream *input_stream, bool ownership);
    void PopScope();

    bool Next(TokenObject *token);

    int Peek() const { return DCHECK_NOTNULL(current_)->ahead; }

    int Move();

private:
    bool ParseDecimalNumberLiteral(int sign, TokenObject *token);

    static bool IsTermination(int ch);
    static inline bool IsNotTermination(int ch) { return !IsTermination(ch); }

    __attribute__ (( __format__ (__printf__, 2, 3)))
    static bool ThrowError(TokenObject *token, const char *fmt, ...);

    static bool VThrowError(TokenObject *token, const char *fmt, va_list ap);

    Scope *current_ = nullptr;
    bool dont_ignore_comments_ = false;

    DISALLOW_IMPLICIT_CONSTRUCTORS(Lexer);
};

} // namespace mio

#endif // MIO_LEXER_H_
