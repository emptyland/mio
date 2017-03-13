#ifndef MIO_PARSER_H_
#define MIO_PARSER_H_

#include "compiler.h"
#include "token.h"
#include <stdarg.h>
#include <string>

namespace mio {

class PackageImporter;
class Statement;
class ValDeclaration;
class VarDeclaration;
class FunctionDefine;
class Return;
class Expression;
class Block;
class IfOperation;

class Lexer;
class TextInputStream;
class TextStreamFactory;
class Scope;
class Zone;

class AstNodeFactory;
class TypeFactory;
class Type;
class Union;
class FunctionPrototype;


class Parser {
public:
    Parser(TypeFactory *types,
           TextStreamFactory *text_streams,
           Scope *scope,
           Zone *zone);
    ~Parser();

    DEF_GETTER(bool, has_error)
    Scope *scope() const { return scope_; }

    ParsingError last_error() const;

    void ClearError();

    TextInputStream *SwitchInputStream(const std::string &key);
    Scope *EnterScope(const std::string &name, int scope_type);
    void EnterScope(Scope *scope);
    Scope *LeaveScope();

    Statement *ParseStatement(bool *ok);
    Statement *ParseDeclaration(bool *ok);
    ValDeclaration *ParseValDeclaration(bool is_export, bool *ok);
    VarDeclaration *ParseVarDeclaration(bool is_export, bool *ok);
    FunctionDefine *ParseFunctionDefine(bool is_export, bool is_native,
                                        bool *ok);
    Return *ParserReturn(bool *ok);
    PackageImporter *ParsePackageImporter(bool *ok);
    Expression *ParseExpression(bool ignore, bool *ok);
    Expression *ParseExpression(bool ignore, int limit, int *rop, bool *ok);
    Block *ParseBlock(bool *ok);
    IfOperation *ParseIfOperation(bool *ok);
    Expression *ParseOperation(int limit, int *rop, bool *ok);
    Expression *ParseSimpleExpression(bool *ok);
    Expression *ParseSuffixed(bool *ok);
    Expression *ParsePrimary(bool *ok);
    Expression *ParseFunctionLiteral(bool *ok);
    Expression *ParseLambda(bool *ok);
    Type *ParseType(bool *ok);
    Union *ParseUnionType(bool *ok);
    FunctionPrototype *ParseFunctionPrototype(bool strict, std::string *name,
                                              bool *ok);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Parser)
private:
    Token Peek() const { return ahead_.token_code(); }

    void Match(Token code, bool *ok) { Match(code, nullptr, ok); }
    void Match(Token code, std::string *txt, bool *ok);

    bool Test(Token code) { return Test(code, nullptr); }
    bool Test(Token code, std::string *txt);

    __attribute__ (( __format__ (__printf__, 2, 3)))
    void ThrowError(const char *fmt, ...);
    void VThrowError(const char *fmt, va_list ap);

    Lexer *lexer_ = nullptr;
    TokenObject ahead_;
    ParsingError last_error_;
    bool has_error_ = false;
    Zone *zone_ = nullptr;
    AstNodeFactory *factory_;
    TypeFactory *types_;
    TextStreamFactory *text_streams_;
    Scope *scope_;
};

} // namespace mio

#endif // MIO_PARSER_H_
