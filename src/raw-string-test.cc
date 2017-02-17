#include "raw-string.h"
#include "gtest/gtest.h"

namespace mio {

TEST(RawStringTest, Sanity) {
    auto z = RawString::kEmpty;
    ASSERT_EQ(0, z->size());
    ASSERT_EQ('\0', z->c_str()[0]);
}

TEST(RawStringTest, Creating) {
    Zone zone;
    auto z = RawString::Create("abc", &zone);
    ASSERT_TRUE(z != nullptr);
    EXPECT_EQ(3, z->size());
    EXPECT_STREQ("abc", z->c_str());
    EXPECT_EQ(8, z->placement_size());
}

TEST(RawStringTest, EmptyCreating) {
    Zone zone;
    auto z = RawString::Create("", &zone);
    EXPECT_EQ(RawString::kEmpty, z);
}

} // namespace mio
