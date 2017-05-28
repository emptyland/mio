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

    ASSERT_EQ(*ref1.data(0), 0x1);
    ASSERT_EQ(*ref1.data(7), 0x1);

    ASSERT_EQ(*ref3.data(0), 0xb);
    ASSERT_EQ(*ref3.data(31), 0xb);
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

TEST(CodeCacheTest, AllChunks) {
    CodeCache cache(16 * 1024);
    ASSERT_TRUE(cache.Init());

    std::vector<mio_buf_t<uint8_t>> chunks;
    cache.GetAllChunks(&chunks);
    ASSERT_EQ(0, chunks.size());

    cache.Allocate(8);
    auto ref2 = cache.Allocate(128);
    cache.Allocate(16);
    cache.Allocate(32);

    chunks.clear();
    cache.GetAllChunks(&chunks);
    ASSERT_EQ(4, chunks.size());
    ASSERT_EQ(8, chunks[0].n);
    ASSERT_EQ(128, chunks[1].n);
    ASSERT_EQ(16, chunks[2].n);
    ASSERT_EQ(32, chunks[3].n);

    cache.Free(ref2);
    chunks.clear();
    cache.GetAllChunks(&chunks);
    ASSERT_EQ(3, chunks.size());
    ASSERT_EQ(8, chunks[0].n);
    ASSERT_EQ(16, chunks[1].n);
    ASSERT_EQ(32, chunks[2].n);
}

TEST(CodeCacheTest, AllIndexs) {
    CodeCache cache(16 * 1024);
    ASSERT_TRUE(cache.Init());

    auto ref1 = cache.Allocate(8);
    auto ref2 = cache.Allocate(128);
    auto ref3 = cache.Allocate(16);
    auto ref4 = cache.Allocate(32);

    std::map<void *, void **> index;
    cache.GetIndexMap(&index);
    ASSERT_EQ(4, index.size());
    ASSERT_EQ(index[ref1.data()], ref1.index());
    ASSERT_EQ(index[ref2.data()], ref2.index());
    ASSERT_EQ(index[ref3.data()], ref3.index());
    ASSERT_EQ(index[ref4.data()], ref4.index());

    cache.Free(ref3);
    index.clear();
    cache.GetIndexMap(&index);
    ASSERT_EQ(3, index.size());
    ASSERT_EQ(index[ref1.data()], ref1.index());
    ASSERT_EQ(index[ref2.data()], ref2.index());
    ASSERT_TRUE(ref3.null());
    ASSERT_EQ(index[ref4.data()], ref4.index());
}

TEST(CodeCacheTest, Compact) {
    CodeCache cache(16 * 1024);
    ASSERT_TRUE(cache.Init());

    auto ref1 = cache.Allocate(8);
    memset(ref1.data(), 0xa, 8);

    auto ref2 = cache.Allocate(128);
    memset(ref2.data(), 0xb, 128);

    auto ref3 = cache.Allocate(16);
    memset(ref3.data(), 0xc, 16);

    auto ref4 = cache.Allocate(32);
    memset(ref4.data(), 0xd, 32);

    cache.Free(ref3);
    std::vector<mio_buf_t<uint8_t>> chunks;
    cache.GetAllChunks(&chunks);
    ASSERT_EQ(3, chunks.size());
    ASSERT_EQ(128, chunks[1].n);
    ASSERT_NE(chunks[2].z, chunks[1].z + chunks[1].n);
    ASSERT_EQ(32, chunks[2].n);

    cache.Compact();
    chunks.clear();
    cache.GetAllChunks(&chunks);
    ASSERT_EQ(3, chunks.size());
    ASSERT_EQ(128, chunks[1].n);
    ASSERT_EQ(chunks[2].z, chunks[1].z + chunks[1].n);
    ASSERT_EQ(32, chunks[2].n);

    ASSERT_EQ(*ref1.data(0), 0xa);
    ASSERT_EQ(*ref1.data(7), 0xa);

    ASSERT_EQ(*ref2.data(0), 0xb);
    ASSERT_EQ(*ref2.data(127), 0xb);

    ASSERT_EQ(*ref4.data(0), 0xd);
    ASSERT_EQ(*ref4.data(31), 0xd);
}

} // namespace mio
