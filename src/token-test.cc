#include "token.h"
#include "gtest/gtest.h"

namespace mio {

TEST(TokenTest, Sanity) {
    TokenObject token;

    token.set_int_data(100L);
    ASSERT_EQ(100L, token.int_data());

    token.set_text("TEXT");
    ASSERT_EQ("TEXT", token.text());
}

} // namespace mio
