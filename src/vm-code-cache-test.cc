#include "vm-code-cache.h"
#include "bit-operations.h"
#include "gtest/gtest.h"

namespace mio {

TEST(CodeCacheTest, Sanity) {
    EXPECT_EQ(32, Bits::FindFirstOne32(0));
    EXPECT_EQ(0, Bits::FindFirstOne32(1));
    EXPECT_EQ(1, Bits::FindFirstOne32(2));
}

TEST(CodeCacheTest, Allocate) {
    CodeCache cache(16 * 1024);
    ASSERT_TRUE(cache.Init());

    auto ref = cache.Allocate(16);
    ASSERT_FALSE(ref.empty());

    ref = cache.Allocate(133);
    ASSERT_FALSE(ref.empty());

    ref = cache.Allocate(32);
    ASSERT_FALSE(ref.empty());
}

} // namespace mio
