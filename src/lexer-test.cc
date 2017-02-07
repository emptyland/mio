#include "lexer.h"
#include "token.h"
#include "fixed-memory-input-stream.h"
#include "gtest/gtest.h"

namespace mio {

TEST(LexerTest, TestingStream) {
    FixedMemoryInputStream s("abc");

    ASSERT_FALSE(s.eof());
    ASSERT_EQ('a', s.ReadOne());

    ASSERT_FALSE(s.eof());
    ASSERT_EQ('b', s.ReadOne());

    ASSERT_FALSE(s.eof());
    ASSERT_EQ('c', s.ReadOne());

    ASSERT_TRUE(s.eof());
}

TEST(LexerTest, AssignAndEq) {
    auto s = new FixedMemoryInputStream("=");

    Lexer lex(s, true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));

    ASSERT_EQ(TOKEN_ASSIGN, token.token_code());
    ASSERT_EQ(0, token.position());
    ASSERT_EQ(1, token.len());

    ASSERT_FALSE(lex.Next(&token));

    lex.PopScope();
    lex.PushScope(new FixedMemoryInputStream("=="), true);

    ASSERT_TRUE(lex.Next(&token));

    ASSERT_EQ(TOKEN_EQ, token.token_code());
    ASSERT_EQ(0, token.position());
    ASSERT_EQ(2, token.len());
}

TEST(LexerTest, IgnoreSpace) {
    Lexer lex(new FixedMemoryInputStream("= =   =  ="), true);
    TokenObject token;

    int pos[4] = {0, 2, 6, 9};

    for (auto i = 0; i < 4; ++i) {
        ASSERT_TRUE(lex.Next(&token));

        ASSERT_EQ(TOKEN_ASSIGN, token.token_code());
        ASSERT_EQ(pos[i], token.position());
    }
    ASSERT_FALSE(lex.Next(&token));
}

TEST(LexerTest, LineComments) {
    Lexer lex(new FixedMemoryInputStream(" #abc\n"), true);
    TokenObject token;

    lex.set_dont_ignore_comments(true);
    ASSERT_TRUE(lex.Next(&token));

    ASSERT_EQ(TOKEN_LINE_COMMENT, token.token_code());
    ASSERT_EQ(1, token.position());
    ASSERT_EQ(5, token.len());
    ASSERT_EQ("#abc\n", token.text());
}

TEST(LexerTest, IntLiteral) {
    Lexer lex(new FixedMemoryInputStream(" 123 "), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));

    ASSERT_EQ(TOKEN_INT_LITERAL, token.token_code());
    ASSERT_EQ(1, token.position());
    ASSERT_EQ(3, token.len());
    ASSERT_EQ("123", token.text());
}

TEST(LexerTest, IntegralSuffix) {
    Lexer lex(new FixedMemoryInputStream(" 233b "), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));

    ASSERT_EQ(TOKEN_I8_LITERAL, token.token_code());
    ASSERT_EQ(1, token.position());
    ASSERT_EQ(4, token.len());
    ASSERT_EQ("233b", token.text());
}

} // namespace mio
