#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "text-input-stream.h"
#include <stdarg.h>

namespace mio {

#define CHECK_OK ok); if (!*ok) { return 0; } ((void)0
#define T(str) RawString::Create((str), zone_)

ParsingError::ParsingError()
    : line(0)
    , column(0)
    , position(0)
    , message("ok") {
}

/*static*/ ParsingError ParsingError::NoError() {
    return ParsingError();
}

Parser::Parser(Zone *zone, TextInputStream *input_stream, bool ownership)
    : zone_(DCHECK_NOTNULL(zone))
    , factory_(new AstNodeFactory(zone))
    , lexer_(new Lexer(input_stream, ownership)) {
    lexer_->Next(&ahead_);
}

Parser::~Parser() {
    delete lexer_;
    delete factory_;
}

ParsingError Parser::last_error() const {
    if (has_error()) {
        return last_error_;
    } else {
        return ParsingError::NoError();
    }
}

void Parser::ClearError() {
    has_error_  = false;
    last_error_ = ParsingError::NoError();
}

Statement *Parser::ParseStatement(bool *ok) {
    int rop = 0;

    switch (Peek()) {
        case TOKEN_WHILE:
            // TODO:
            break;

        case TOKEN_FOR:
            // TODO:
            break;

        default:
            return ParseExpression(0, &rop, ok);
    }

    return nullptr;
}

/*
 * package_define_import = package_define [ import_statment ]
 * package_define = "package" id
 * import_statment = "with" "(" import_item { import_item } ")"
 * import_item = string_literal [ "as" import_alias ]
 * import_alias = "_" | id | "."
 */
PackageImporter *Parser::ParsePackageImporter(bool *ok) {
    auto position = ahead_.position();
    Match(TOKEN_PACKAGE, CHECK_OK);

    std::string txt;
    Match(TOKEN_ID, &txt, CHECK_OK);
    auto node = factory_->CreatePackageImporter(txt, position);

    if (!Test(TOKEN_WITH)) {
        return node;
    }
    Match(TOKEN_LPAREN, CHECK_OK);

    while (!Test(TOKEN_RPAREN)) {
        Match(TOKEN_STRING_LITERAL, &txt, CHECK_OK);
        auto import_name = txt;

        if (!Test(TOKEN_AS)) {
            node->mutable_import_list()->Put(T(import_name), RawString::kEmpty);
            continue;
        }

        if (Test(TOKEN_DOT)) {
            node->mutable_import_list()->Put(T(import_name), T("."));
        } else {
            Match(TOKEN_ID, &txt, CHECK_OK);
            node->mutable_import_list()->Put(T(import_name), T(txt));
        }
    }
    return node;
}

Expression *Parser::ParseExpression(int limit, int *rop, bool *ok) {
    switch (Peek()) {
        case TOKEN_LBRACE: // {
            // ParseBlockExpression
            break;

        case TOKEN_ID:
        case TOKEN_I8_LITERAL:
        case TOKEN_I16_LITERAL:
        case TOKEN_I32_LITERAL:
        case TOKEN_INT_LITERAL:
        case TOKEN_F32_LITERAL:
        case TOKEN_F64_LITERAL:
        case TOKEN_STRING_LITERAL:
        case TOKEN_MINUS:
        case TOKEN_WAVE:
            return ParseOperation(limit, rop, ok);

        case TOKEN_IF:
            *rop = OP_OTHER;
            return ParseIfOperation(ok);

        default:
            *ok = false;
            if (Peek() == TOKEN_ERROR) {
                ThrowError("unexpected expression, lexer error: %s",
                           ahead_.text().c_str());
            } else {
                ThrowError("unexpected expression, expected: %s",
                           ahead_.ToNameWithText().c_str());
            }
            break;
    }
    return nullptr;
}

// if ( condition) then_statement else else_statement
IfOperation *Parser::ParseIfOperation(bool *ok) {
    auto position = ahead_.position();
    Match(TOKEN_IF, CHECK_OK);

    Match(TOKEN_LPAREN, CHECK_OK);
    int rop;
    auto expr = ParseExpression(0, &rop, CHECK_OK);
    Match(TOKEN_RPAREN, CHECK_OK);

    auto then_stmt = ParseStatement(CHECK_OK);
    Statement *else_stmt = nullptr;
    if (Test(TOKEN_ELSE)) {
        else_stmt = ParseStatement(CHECK_OK);
    }

    auto node = factory_->CreateIfOperation(expr, then_stmt, else_stmt, position);
    return node;
}

// 1 + 2 * i
// node = SmiLiteral(1)
// op = +
// rhs =
// -- node = SmiLiteral(2)
// -- op = *
// -- rhs = Symbol(i)
//
// 2 * i + 1
// node = SmiLiteral(2)
// op = *
// rhs =
// -- node = Symbol(i)
// -- op = +
//
Expression *Parser::ParseOperation(int limit, int *rop, bool *ok) {
    auto position = ahead_.position();
    auto op = TokenToUnaryOperator(Peek());
    if (op != OP_NOT_UNARY) {
        lexer_->Next(&ahead_);

        auto operand = ParseExpression(limit, rop, CHECK_OK);
        *rop = op;
        return factory_->CreateUnaryOperation(op, operand, position);
    }

    auto node = ParseSimpleExpression(CHECK_OK);
    if (Test(TOKEN_ASSIGN)) { // = assignment
        if (!node->is_lval()) {
            ThrowError("assigment target is not lval");
            *ok = false;
            return nullptr;
        }
        auto rval = ParseExpression(0, rop, CHECK_OK);
        *rop = OP_OTHER;

        return factory_->CreateAssignment(node, rval, position);
    }

    op = TokenToBinaryOperator(Peek());
    while (op != OP_NOT_BINARY && GetOperatorPriority(op)->left > limit) {
        lexer_->Next(&ahead_);
        position = ahead_.position();

        int next_op;
        auto rhs = ParseExpression(GetOperatorPriority(op)->right, &next_op,
                                   CHECK_OK);
        node = factory_->CreateBinaryOperation(op, node, rhs, position);

        op = static_cast<Operator>(next_op);
    }
    *rop = op;
    return node;
}

Expression *Parser::ParseSimpleExpression(bool *ok) {
    auto position = ahead_.position();
    switch (Peek()) {
        case TOKEN_TRUE:
            lexer_->Next(&ahead_);
            return factory_->CreateI1SmiLiteral(true, position);

        case TOKEN_FALSE:
            lexer_->Next(&ahead_);
            return factory_->CreateI1SmiLiteral(false, position);

        case TOKEN_INT_LITERAL:
            lexer_->Next(&ahead_);
            return factory_->CreateI64SmiLiteral(ahead_.int_data(), position);

        default:
            return ParseSuffixed(ok);
    }
}

Expression *Parser::ParseSuffixed(bool *ok) {
    auto position = ahead_.position();
    auto node = ParsePrimary(CHECK_OK);
    int rop = 0;

    for (;;) {
        switch (Peek()) {
            // expr ( a1, b1, c1)
            case TOKEN_LPAREN: {
                auto args = new (zone_) ZoneVector<Expression *>(zone_);
                Match(TOKEN_LPAREN, CHECK_OK);
                if (!Test(TOKEN_RPAREN)) {
                    auto arg = ParseExpression(0, &rop, CHECK_OK);
                    args->Add(arg);
                    while (Test(TOKEN_COMMA)) {
                        arg = ParseExpression(0, &rop, CHECK_OK);
                        args->Add(arg);
                    }
                    Match(TOKEN_RPAREN, CHECK_OK);
                }
                node = factory_->CreateCall(node, args, position);
            } break;

            // expr . field . field
            case TOKEN_DOT: {
                Match(TOKEN_DOT, CHECK_OK);

                std::string field_name;
                Match(TOKEN_ID, &field_name, CHECK_OK);

                node = factory_->CreateFieldAccessing(field_name, node, position);
            } break;

            default:
                return node;
        }
    }
}

Expression *Parser::ParsePrimary(bool *ok) {
    auto position = ahead_.position();
    switch (Peek()) {
         // ( expression )
        case TOKEN_LPAREN: {
            Match(TOKEN_LPAREN, CHECK_OK);
            int rop = 0;
            auto node = ParseExpression(0, &rop, CHECK_OK);
            Match(TOKEN_RPAREN, CHECK_OK);
            return node;
        } break;

        // symbol
        // namespace::symbol
        case TOKEN_ID: {
            std::string id, ns;
            Match(TOKEN_ID, &id, CHECK_OK);
            if (Test(TOKEN_NAME_BREAK)) { // ::
                ns = id;
                Match(TOKEN_ID, &id, CHECK_OK);
            }
            return factory_->CreateSymbol(id, ns, position);
        } break;

        default:
            ThrowError("unexpected \'expression\', expected %s",
                       TokenNameWithText(Peek()).c_str());
            break;

    }
    *ok = false;
    return nullptr;
}

void Parser::Match(Token code, std::string *txt, bool *ok) {
    DCHECK_NE(TOKEN_ERROR, code);

    if (ahead_.token_code() != code) {
        *ok = false;
        if (ahead_.is_error()) {
            ThrowError("unexpected: %s, lexer error: %s",
                       TokenNameWithText(code).c_str(),
                       ahead_.text().c_str());
        } else {
            ThrowError("unexpected: %s, expected: %s",
                       TokenNameWithText(code).c_str(),
                       ahead_.ToNameWithText().c_str());
        }
    }
    if (txt) {
        txt->assign(ahead_.text());
    }
    lexer_->Next(&ahead_);
}

bool Parser::Test(Token code, std::string *txt) {
    if (ahead_.token_code() != code) {
        return false;
    }

    if (txt) {
        txt->assign(ahead_.text());
    }
    lexer_->Next(&ahead_);
    return true;
}

void Parser::ThrowError(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    VThrowError(fmt, ap);
    va_end(ap);
}

void Parser::VThrowError(const char *fmt, va_list ap) {
    has_error_ = true;
    last_error_.column    = lexer_->current()->column;
    last_error_.line      = lexer_->current()->line;
    last_error_.position  = lexer_->current()->position;
    last_error_.file_name = lexer_->input_stream()->file_name();

    va_list copied;
    int len = 512, rv = len;
    std::string buf;
    do {
        len = rv + 512;
        buf.resize(len, 0);
        va_copy(copied, ap);
        rv = vsnprintf(&buf[0], len, fmt, ap);
        va_copy(ap, copied);
    } while (rv > len);
    buf.resize(strlen(buf.c_str()), 0);
    last_error_.message = std::move(buf);
}

} // namespace mio
