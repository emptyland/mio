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
    ASSERT_EQ(123, token.int_data());
    ASSERT_EQ(1, token.position());
    ASSERT_EQ(3, token.len());
    ASSERT_EQ("123", token.text());
}

TEST(LexerTest, IntegralSuffix) {
    Lexer lex(new FixedMemoryInputStream(" 64b "), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));

    ASSERT_EQ(TOKEN_I8_LITERAL, token.token_code());
    ASSERT_EQ(64, token.i8_data());
    ASSERT_EQ(1, token.position());
    ASSERT_EQ(3, token.len());
    ASSERT_EQ("64b", token.text());
}

TEST(LexerTest, HexIntegral) {
    Lexer lex(new FixedMemoryInputStream("0x1 0x001 0x00001"), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_I8_LITERAL, token.token_code());
    ASSERT_EQ(1, token.i8_data());
    ASSERT_EQ(0, token.position());
    ASSERT_EQ(3, token.len());
    ASSERT_EQ("0x1", token.text());

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_I16_LITERAL, token.token_code());
    ASSERT_EQ(1, token.i8_data());
    ASSERT_EQ(4, token.position());
    ASSERT_EQ(5, token.len());
    ASSERT_EQ("0x001", token.text());

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_I32_LITERAL, token.token_code());
    ASSERT_EQ(1, token.i8_data());
    ASSERT_EQ(10, token.position());
    ASSERT_EQ(7, token.len());
    ASSERT_EQ("0x00001", token.text());

    ASSERT_FALSE(lex.Next(&token));
}

TEST(LexerTest, IDParsing) {
    Lexer lex(new FixedMemoryInputStream("$1 _1 name"), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_ID, token.token_code());
    ASSERT_EQ(0, token.position());
    ASSERT_EQ(2, token.len());
    ASSERT_EQ("$1", token.text());

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_ID, token.token_code());
    ASSERT_EQ(3, token.position());
    ASSERT_EQ(2, token.len());
    ASSERT_EQ("_1", token.text());

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_ID, token.token_code());
    ASSERT_EQ(6, token.position());
    ASSERT_EQ(4, token.len());
    ASSERT_EQ("name", token.text());

    ASSERT_FALSE(lex.Next(&token));
}

TEST(LexerTest, IDKeywordParsing) {
    Lexer lex(new FixedMemoryInputStream("i8 and $1"), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_I8, token.token_code());
    ASSERT_EQ(0, token.position());
    ASSERT_EQ(2, token.len());
    ASSERT_EQ("i8", token.text());

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_AND, token.token_code());
    ASSERT_EQ(3, token.position());
    ASSERT_EQ(3, token.len());
    ASSERT_EQ("and", token.text());

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_ID, token.token_code());
    ASSERT_EQ(7, token.position());
    ASSERT_EQ(2, token.len());
    ASSERT_EQ("$1", token.text());

    ASSERT_FALSE(lex.Next(&token));
}

TEST(LexerTest, StringLiteral) {
    Lexer lex(new FixedMemoryInputStream("\'\' \'abc\'"), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_STRING_LITERAL, token.token_code());
    ASSERT_EQ(0, token.position());
    ASSERT_EQ(2, token.len());
    ASSERT_EQ("", token.text());

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_STRING_LITERAL, token.token_code());
    ASSERT_EQ(3, token.position());
    ASSERT_EQ(5, token.len());
    ASSERT_EQ("abc", token.text());
}

TEST(LexerTest, StringLiteralHexEscape) {
    Lexer lex(new FixedMemoryInputStream("\'\\x00\\x01\'"), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_STRING_LITERAL, token.token_code()) << token.text();
    ASSERT_EQ(0, token.position());
    ASSERT_EQ(10, token.len());
    EXPECT_EQ(0x00, token.text()[0]);
    EXPECT_EQ(0x01, token.text()[1]);
}

TEST(LexerTest, StringLiteralSpecEscape) {
    Lexer lex(new FixedMemoryInputStream("\'\\r \\n \\t'"), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));
    ASSERT_EQ(TOKEN_STRING_LITERAL, token.token_code()) << token.text();
    ASSERT_EQ(0, token.position());
    ASSERT_EQ(10, token.len());
    EXPECT_EQ("\r \n \t", token.text());
}

TEST(LexerTest, Operators1) {
    Lexer lex(new FixedMemoryInputStream("< << <= <>"), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));
    EXPECT_EQ(TOKEN_LT, token.token_code()) << token.text();
    EXPECT_EQ(0, token.position());
    EXPECT_EQ(1, token.len());

    ASSERT_TRUE(lex.Next(&token));
    EXPECT_EQ(TOKEN_LSHIFT, token.token_code()) << token.text();
    EXPECT_EQ(2, token.position());
    EXPECT_EQ(2, token.len());

    ASSERT_TRUE(lex.Next(&token));
    EXPECT_EQ(TOKEN_LE, token.token_code()) << token.text();
    EXPECT_EQ(5, token.position());
    EXPECT_EQ(2, token.len());

    ASSERT_TRUE(lex.Next(&token));
    EXPECT_EQ(TOKEN_NE, token.token_code()) << token.text();
    EXPECT_EQ(8, token.position());
    EXPECT_EQ(2, token.len());

    ASSERT_FALSE(lex.Next(&token));
}

TEST(LexerTest, Operators2) {
    Lexer lex(new FixedMemoryInputStream("> |> >> >="), true);
    TokenObject token;

    ASSERT_TRUE(lex.Next(&token));
    EXPECT_EQ(TOKEN_GT, token.token_code()) << token.text();
    EXPECT_EQ(0, token.position());
    EXPECT_EQ(1, token.len());

    ASSERT_TRUE(lex.Next(&token));
    EXPECT_EQ(TOKEN_RSHIFT_L, token.token_code()) << token.text();
    EXPECT_EQ(2, token.position());
    EXPECT_EQ(2, token.len());

    ASSERT_TRUE(lex.Next(&token));
    EXPECT_EQ(TOKEN_RSHIFT_A, token.token_code()) << token.text();
    EXPECT_EQ(5, token.position());
    EXPECT_EQ(2, token.len());

    ASSERT_TRUE(lex.Next(&token));
    EXPECT_EQ(TOKEN_GE, token.token_code()) << token.text();
    EXPECT_EQ(8, token.position());
    EXPECT_EQ(2, token.len());

    ASSERT_FALSE(lex.Next(&token));
}

} // namespace mio
