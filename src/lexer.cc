#include "lexer.h"
#include "token.h"
#include "text-input-stream.h"
#include "number-parser.h"
//#include <memory>

namespace mio {

Lexer::Lexer(TextInputStream *input_stream, bool ownership) {
    PushScope(input_stream, ownership);
}

Lexer::~Lexer() {
    while (current_) {
        PopScope();
    }
}

void Lexer::PushScope(TextInputStream *input_stream, bool ownership) {
    auto scope = new Scope;
    scope->input_stream = input_stream;
    scope->ownership    = ownership;

    scope->ahead = scope->input_stream->ReadOne();
    if (!scope->input_stream->eof()) {
        scope->column = 1;
        scope->line   = 1;
    }

    scope->top = current_;
    current_ = scope;
}

void Lexer::PopScope() {
    if (!current_) {
        return;
    }

    auto top = current_->top;
    if (current_->ownership) {
        delete current_->input_stream;
    }
    delete current_;
    current_ = top;
}

bool Lexer::Next(TokenObject *token) {
    token->set_token_code(TOKEN_ERROR);

    for (;;) {
        auto ahead = Peek();

        switch (ahead) {
            case -1: // EOF
                token->set_token_code(TOKEN_EOF);
                token->set_len(0);
                token->set_position(current()->position);
                return false;

            case '-':
                token->set_position(current()->position);
                ahead = Move();
                if (isdigit(ahead)) {
                    token->mutable_text()->assign("-");

                    return ParseDecimalNumberLiteral(-1, token);
                } else {
                    token->set_token_code(TOKEN_MINUS);
                    token->set_len(1);
                    return true;
                }
                break;

            case '=':
                token->set_position(current()->position);

                ahead = Move();
                if (ahead == '=') {
                    Move();
                    token->set_token_code(TOKEN_EQ);
                    token->set_len(2);
                } else {
                    token->set_token_code(TOKEN_ASSIGN);
                    token->set_len(1);
                }
                return true;

            case '#': {
                if (dont_ignore_comments_) {
                    token->set_token_code(TOKEN_LINE_COMMENT);
                    token->mutable_text()->assign("#");
                    token->set_position(current()->position);
                }

                Move();
                for (;;) {
                    auto ch = Peek();
                    if (ch == '\n') {
                        Move();
                        if (dont_ignore_comments_) {
                            token->mutable_text()->append(1, ch);
                            token->set_len(current()->position - token->position());
                            return true;
                        } else {
                            break;
                        }
                    } else if (ch == -1) {
                        if (dont_ignore_comments_) {
                            token->set_len(current()->position - token->position());
                            return true;
                        } else {
                            break;
                        }
                    } else {
                        Move();
                        if (dont_ignore_comments_) {
                            token->mutable_text()->append(1, ch);
                        }
                    }
                }
            } break;

            case '0':
                token->set_position(current()->position);
                token->mutable_text()->assign("0");
                ahead = Move();
                if (ahead == 'x' || ahead == 'X') {
                    token->mutable_text()->append(1, ahead);
                    Move();
                    return ParseHexadecimalNumberLiteral(token);
                } else {
                    return ParseDecimalNumberLiteral(1, token);
                }
                break;

            default:
                if (isspace(ahead)) {
                    while (isspace(Move()))
                        ;

                } else if (isdigit(ahead)) {
                    token->mutable_text()->clear();
                    token->set_position(current()->position);
                    return ParseDecimalNumberLiteral(1, token);
                }
                break;
        }
    }
}

int Lexer::Move() {
    DCHECK_NE(current_, static_cast<Scope*>(nullptr)) << "forget push scope?";

    current_->ahead = current_->input_stream->ReadOne();
    if (current_->ahead == '\n') {
        ++current_->line;
    }
    ++current_->column;
    ++current_->position;
    return current_->ahead;
}

// stuffix:
// b - i8
// w - i16
// d - i32
// q - i64
//
// F - f32
// D - f64
bool Lexer::ParseDecimalNumberLiteral(int sign, TokenObject *token) {
    bool has_dot = false;
    int ahead = 0;
    do {
        ahead = Peek();

        if (isdigit(ahead)) {
            token->mutable_text()->append(1, ahead);
            ahead = Move();
        } else if (ahead == '.') {
            token->mutable_text()->append(1, ahead);
            if (has_dot) {
                return ThrowError(token, "duplicated dot in number literal.");
            } else {
                has_dot = true;
            }
            ahead = Move();
        } else if (ahead == 'b' ||
                   ahead == 'w' ||
                   ahead == 'd' ||
                   ahead == 'q'){
            if (has_dot) {
                return ThrowError(token, "floating number has integral suffix:"
                                  " %c.", ahead);
            }
            return ParseDecimalIntegralValue(&ahead, token);
        } else if (ahead == 'F' ||
                   ahead == 'D') {
            return ParseDecimalFloatingValue(&ahead, token);
        } else {
            return ThrowError(token, "incorrect decimal number literal, "
                              "unexpected character \'%c\'.", ahead);
        }

    } while (IsNotTermination(ahead));

    bool ok = true;
    if (has_dot) {
        token->set_token_code(TOKEN_F32_LITERAL);
        token->set_f32_data(NumberParser::ParseF32(token->text().c_str()));
    } else {
        token->set_token_code(TOKEN_INT_LITERAL);
        token->set_int_data(NumberParser::ParseDecimalInt(token->text().data(),
                                                          token->text().size(),
                                                          &ok));
    }
    if (!ok) {
        auto literal = token->text();
        return ThrowError(token, "incorrect integral number literal %s",
                          literal.c_str());
    }
    token->set_len(current()->position - token->position());
    return true;
}

bool Lexer::ParseDecimalIntegralValue(int *ahead, TokenObject *token) {
    bool ok = true;
    switch (*ahead) {
        case 'b':
            token->set_token_code(TOKEN_I8_LITERAL);
            token->set_i8_data(NumberParser::ParseDecimalI8(token->text().data(),
                                                            token->text().size(),
                                                            &ok));
            break;
        case 'w':
            token->set_token_code(TOKEN_I16_LITERAL);
            token->set_i16_data(NumberParser::ParseDecimalI16(token->text().data(),
                                                              token->text().size(),
                                                              &ok));
            break;
        case 'd':
            token->set_token_code(TOKEN_I32_LITERAL);
            token->set_i32_data(NumberParser::ParseDecimalI32(token->text().data(),
                                                              token->text().size(),
                                                              &ok));
            break;
        case 'q':
            token->set_token_code(TOKEN_I64_LITERAL);
            token->set_i64_data(NumberParser::ParseDecimalI64(token->text().data(),
                                                              token->text().size(),
                                                              &ok));
            break;
        default:
            break;
    }
    if (!ok) {
        auto literal = token->text();
        return ThrowError(token, "incorrect integral number literal %s",
                          literal.c_str());
    }

    token->mutable_text()->append(1, *ahead);
    if (IsNotTermination(*ahead = Move())) {
        return ThrowError(token, "incorrect integral number literal.");
    }
    token->set_len(current()->position - token->position());
    return true;
}

bool Lexer::ParseDecimalFloatingValue(int *ahead, TokenObject *token) {
    switch (*ahead) {
        case 'F':
            token->set_token_code(TOKEN_F32_LITERAL);
            token->set_f32_data(NumberParser::ParseF32(token->text().c_str()));
            break;
        case 'D':
            token->set_token_code(TOKEN_F64_LITERAL);
            token->set_f64_data(NumberParser::ParseF64(token->text().c_str()));
            break;
        default:
            break;
    }
    token->mutable_text()->append(1, *ahead);
    if (IsNotTermination(*ahead = Move())) {
        return ThrowError(token, "incorrect floating number literal.");
    }
    token->set_len(current()->position - token->position());
    return true;
}

bool Lexer::ParseHexadecimalNumberLiteral(TokenObject *token) {
    int ahead = 0;
    do {
        ahead = Peek();
        if (isdigit(ahead) ||
            (ahead >= 'A' && ahead <= 'F') ||
            (ahead >= 'a' && ahead <= 'f')) {

            token->mutable_text()->append(1, ahead);
            ahead = Move();
        } else {
            return ThrowError(token, "incorrect hexadecimal number literal, "
                              "unexpected character \'%c\'.", ahead);
        }

        if (token->text().size() > sizeof("0xffffffffffffffff") - 1) {
            return ThrowError(token, "hexadecimal number literal too large.");
        }
    } while (IsNotTermination(ahead));

    if (token->text().size() < sizeof("0x0") - 1) {
        return ThrowError(token, "incorrect hexadecimal number literal.");
    }

    // 1 ~ 2 -> i8
    // 3 ~ 4 -> i16
    // 5 ~ 8 -> i32
    // 9 ~ 16 -> i64
    bool ok = true;
    switch (token->text().size() - 2) { // strlen("0x") == 2
        case 1: case 2:
            token->set_token_code(TOKEN_I8_LITERAL);
            token->set_i8_data(NumberParser::ParseHexadecimalI8(token->text().data() + 2,
                                                                token->text().size() - 2,
                                                                &ok));
            break;

        case 3: case 4:
            token->set_token_code(TOKEN_I16_LITERAL);
            token->set_i16_data(NumberParser::ParseHexadecimalI16(token->text().data() + 2,
                                                                  token->text().size() - 2,
                                                                  &ok));
            break;

        case 5: case 6: case 7: case 8:
            token->set_token_code(TOKEN_I32_LITERAL);
            token->set_i32_data(NumberParser::ParseHexadecimalI32(token->text().data() + 2,
                                                                  token->text().size() - 2,
                                                                  &ok));
            break;

        case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16:
            token->set_token_code(TOKEN_I64_LITERAL);
            token->set_i64_data(NumberParser::ParseHexadecimalI64(token->text().data() + 2,
                                                                  token->text().size() - 2,
                                                                  &ok));
            break;

        default:
            break;
    }
    if (!ok) {
        auto literal = token->text();
        return ThrowError(token, "incorrect integral number literal %s",
                          literal.c_str());
    }
    token->set_len(current()->position - token->position());
    return true;
}

/*static*/ bool Lexer::IsTermination(int ch) {
    switch (ch) {
        case -1:
            return true;

        case '{': case '}': case '(': case ')':
            // TODO:
            return true;

        default:
            if (isspace(ch)) {
                return true;
            }
            break;
    }
    return false;
}

/*static*/ bool Lexer::ThrowError(TokenObject *token, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto rv = VThrowError(token, fmt, ap);
    va_end(ap);
    return rv;
}

/*static*/
bool Lexer::VThrowError(TokenObject *token, const char *fmt, va_list ap) {
    token->set_token_code(TOKEN_ERROR);
    token->mutable_text()->clear();

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

    token->mutable_text()->assign(std::move(buf));
    return true;
}

} // namespace mio
