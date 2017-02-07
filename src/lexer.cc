#include "lexer.h"
#include "token.h"
#include "text-input-stream.h"
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

    //return 0;
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
    int ahead = 9;
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
            switch (ahead) {
                case 'b':
                    token->set_token_code(TOKEN_I8_LITERAL);
                    break;
                case 'w':
                    token->set_token_code(TOKEN_I16_LITERAL);
                    break;
                case 'd':
                    token->set_token_code(TOKEN_I32_LITERAL);
                    break;
                case 'q':
                    token->set_token_code(TOKEN_I64_LITERAL);
                    break;
                default:
                    break;
            }
            token->mutable_text()->append(1, ahead);
            if (IsNotTermination(ahead = Move())) {
                return ThrowError(token, "incorrect integral number literal.");
            }
            token->set_len(current()->position - token->position());
            return true;
        } else if (ahead == 'F' || ahead == 'D') {
            switch (ahead) {
                case 'F':
                    token->set_token_code(TOKEN_F32_LITERAL);
                    break;
                case 'D':
                    token->set_token_code(TOKEN_F64_LITERAL);
                    break;
                default:
                    break;
            }
            token->mutable_text()->append(1, ahead);
            if (IsNotTermination(ahead = Move())) {
                return ThrowError(token, "incorrect floating number literal.");
            }
            token->set_len(current()->position - token->position());
            return true;
        } else {
            return ThrowError(token, "incorrect number literal.");
        }

    } while (IsNotTermination(ahead));

    if (has_dot) {
        token->set_token_code(TOKEN_F32_LITERAL);
    } else {
        token->set_token_code(TOKEN_INT_LITERAL);
    }
    token->set_len(current()->position - token->position());
    return true;
}

/* static */ bool Lexer::IsTermination(int ch) {
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

/* static */ bool Lexer::ThrowError(TokenObject *token, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto rv = VThrowError(token, fmt, ap);
    va_end(ap);
    return rv;
}

/* static */
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
