#include "number-parser.h"
#include "gtest/gtest.h"

namespace mio {

TEST(NumberParser, Sanity) {
    bool ok = true;
    ASSERT_EQ(0, NumberParser::ParseDecimalI8("0", 1, &ok));
    ASSERT_TRUE(ok);
}

TEST(NumberParser, I8Parsing) {
    bool ok = true;
    EXPECT_EQ(127, NumberParser::ParseDecimalI8("127", 3, &ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(-128, NumberParser::ParseDecimalI8("-128", 4, &ok));
    EXPECT_TRUE(ok);
}

TEST(NumberParser, I8TooBigTooSmallNegative) {
    bool ok = true;
    EXPECT_EQ(0, NumberParser::ParseDecimalI8("128", 3, &ok));
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_EQ(0, NumberParser::ParseDecimalI8("-129", 4, &ok));
    EXPECT_FALSE(ok);
}

TEST(NumberParser, I8IncorrectCharNegative) {
    bool ok = true;
    EXPECT_EQ(0, NumberParser::ParseDecimalI8("12b", 3, &ok));
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_EQ(0, NumberParser::ParseDecimalI8("-12 ", 4, &ok));
    EXPECT_FALSE(ok);
}

TEST(NumberParser, I16Parsing) {
    bool ok = true;
    EXPECT_EQ(32767, NumberParser::ParseDecimalI16("32767", 5, &ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(-32768, NumberParser::ParseDecimalI16("-32768", 6, &ok));
    EXPECT_TRUE(ok);
}

TEST(NumberParser, I32Parsing) {
    bool ok = true;
    EXPECT_EQ(2147483647, NumberParser::ParseDecimalI32("2147483647", 10, &ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(-2147483648, NumberParser::ParseDecimalI32("-2147483648", 11, &ok));
    EXPECT_TRUE(ok);
}

TEST(NumberParser, HexI8Parsing) {
    bool ok = true;
    EXPECT_EQ(127, NumberParser::ParseHexadecimalI8("7f", 2, &ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(-128, NumberParser::ParseHexadecimalI8("80", 2, &ok));
    EXPECT_TRUE(ok);
}

TEST(NumberParser, HexI64Parsing) {
    bool ok = true;
    EXPECT_EQ(0, NumberParser::ParseHexadecimalI64("0", 1, &ok));
    EXPECT_TRUE(ok);

    ok = true;
    EXPECT_EQ(-1, NumberParser::ParseHexadecimalI64("ffffffffffffffff", 16, &ok));
    EXPECT_TRUE(ok);
}

} // namespace mio
