#ifndef MIO_PARSER_H_
#define MIO_PARSER_H_

#include "token.h"
#include <stdarg.h>
#include <string>

namespace mio {

class PackageImporter;
class Statement;
class Expression;
class IfOperation;

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

    Statement *ParseStatement(bool *ok);
    PackageImporter *ParsePackageImporter(bool *ok);
    Expression *ParseExpression(int limit, int *rop, bool *ok);
    IfOperation *ParseIfOperation(bool *ok);
    Expression *ParseOperation(int limit, int *rop, bool *ok);
    Expression *ParseSimpleExpression(bool *ok);
    Expression *ParseSuffixed(bool *ok);
    Expression *ParsePrimary(bool *ok);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Parser)
private:
    Token Peek() const { return ahead_.token_code(); }

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
