#include "zone.h"
#include "gtest/gtest.h"
#include <vector>

namespace mio {

TEST(ZoneTest, Sanity) {
    Zone zone;

    zone.AssertionTest();
    zone.PreheatEverySlab();
}

TEST(ZoneTest, FixedSizeAllocated) {
    Zone zone;
    zone.PreheatEverySlab();

    std::vector<int *> blocks;
    const auto round = zone.slab_max_chunks(0);
    for (int i = 0; i < round + 1; ++i) {
        auto block = zone.Allocate(Zone::kMinAllocatedSize);
        ASSERT_TRUE(block != nullptr);

        Round32BytesFill(i, block, Zone::kMinAllocatedSize);
        blocks.push_back(static_cast<int *>(block));
    }

    for (int i = 0; i < round + 1; ++i) {
        auto block = blocks[i];
        EXPECT_EQ(i, block[0]);
        EXPECT_EQ(i, block[3]);
    }
}

TEST(ZoneTest, Reallocate) {
    Zone zone;

    auto c1 = zone.Allocate(32);
    auto c2 = zone.Allocate(32);

    ASSERT_NE(c1, c2);

    auto c3 = zone.Allocate(32);

    ASSERT_NE(c1, c3);
    ASSERT_NE(c2, c3);

    zone.Free(c2);

    auto c4 = zone.Allocate(32);

    ASSERT_EQ(c4, c2);
}

} // namespace mio
