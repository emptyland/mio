#ifndef MIO_PARSER_H_
#define MIO_PARSER_H_

#include "token.h"
#include <stdarg.h>
#include <string>

namespace mio {

class PackageImporter;

class Lexer;
class TextInputStream;
class Zone;
class AstNodeFactory;

struct ParsingError {
    int column;
    int line;
    int position;
    std::string file_name;
    std::string message;

    ParsingError();

    static ParsingError NoError();
}; // struct ParsingError

class Parser {
public:
    Parser(Zone *zone, TextInputStream *input_stream, bool ownership);
    ~Parser();

    DEF_GETTER(bool, has_error)

    ParsingError last_error() const;

    void ClearError();

    PackageImporter *ParsePackageImporter(bool *ok);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Parser)
private:
    void Match(Token code, bool *ok) { Match(code, nullptr, ok); }
    void Match(Token code, std::string *txt, bool *ok);

    bool Test(Token code) { return Test(code, nullptr); }
    bool Test(Token code, std::string *txt);

    void ThrowError(const char *fmt, ...);
    void VThrowError(const char *fmt, va_list ap);

    Lexer *lexer_ = nullptr;
    TokenObject ahead_;
    ParsingError last_error_;
    bool has_error_ = false;
    Zone *zone_ = nullptr;
    AstNodeFactory *factory_;
};

} // namespace mio

#endif // MIO_PARSER_H_
