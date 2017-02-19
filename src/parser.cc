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

void Parser::Match(Token code, std::string *txt, bool *ok) {
    if (ahead_.token_code() != code) {
        *ok = false;
        ThrowError("unexpected: %s, expected: %s",
                   kTokenName2Text[code],
                   kTokenName2Text[ahead_.token_code()]);
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
