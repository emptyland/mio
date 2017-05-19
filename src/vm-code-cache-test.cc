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

TEST(CodeCacheTest, Bitmap) {
    CodeCache cache(16 * 1024);
    ASSERT_TRUE(cache.Init());

    auto ref = cache.Allocate(8);
    ASSERT_FALSE(ref.empty());
    ASSERT_TRUE(cache.bitmap_test(0));
    ASSERT_TRUE(cache.bitmap_test(1));

    ref = cache.Allocate(16);
    ASSERT_FALSE(ref.empty());
    ASSERT_TRUE(cache.bitmap_test(2));
    ASSERT_TRUE(cache.bitmap_test(5));

    ref = cache.Allocate(32);
    ASSERT_FALSE(ref.empty());
    ASSERT_TRUE(cache.bitmap_test(6));
    ASSERT_TRUE(cache.bitmap_test(13));
}

TEST(CodeCacheTest, AllocateOnHole) {
    CodeCache cache(16 * 1024);
    ASSERT_TRUE(cache.Init());

    auto ref1 = cache.Allocate(8);
    memset(ref1.data(), 0x1, 8);

    auto ref2 = cache.Allocate(16);
    memset(ref2.data(), 0xa, 16);

    auto ref3 = cache.Allocate(32);
    memset(ref3.data(), 0xb, 32);

    auto data = ref2.data();
    cache.Free(ref2);
    ASSERT_FALSE(cache.bitmap_test(2));
    ASSERT_FALSE(cache.bitmap_test(5));

    auto again = cache.Allocate(16);
    ASSERT_EQ(data, again.data());
    ASSERT_TRUE(cache.bitmap_test(2));
    ASSERT_TRUE(cache.bitmap_test(5));
}

TEST(CodeCacheTest, AllocateSkipHole) {
    CodeCache cache(16 * 1024);
    ASSERT_TRUE(cache.Init());

    auto ref1 = cache.Allocate(8);
    memset(ref1.data(), 0x1, 8);

    auto ref2 = cache.Allocate(16);
    memset(ref2.data(), 0xa, 16);

    auto ref3 = cache.Allocate(32);
    memset(ref3.data(), 0xb, 32);

    auto data = ref2.data();
    cache.Free(ref2);
    ASSERT_FALSE(cache.bitmap_test(2));
    ASSERT_FALSE(cache.bitmap_test(5));

    auto again = cache.Allocate(48);
    ASSERT_NE(data, again.data());
    ASSERT_FALSE(cache.bitmap_test(2));
    ASSERT_FALSE(cache.bitmap_test(5));

    ASSERT_TRUE(cache.bitmap_test(14));
    ASSERT_TRUE(cache.bitmap_test(25));
}

} // namespace mio
