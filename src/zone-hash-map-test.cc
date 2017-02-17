#include "zone-hash-map.h"
#include "gtest/gtest.h"

namespace mio {

typedef ZoneHashMap<int, int> IntHashMap;

TEST(ZoneHashMapTest, Sanity) {
    Zone zone;
    IntHashMap map(&zone);

    bool has_insert = false;
    auto pair = map.GetOrInsert(1, &has_insert);
    ASSERT_TRUE(pair != nullptr);
    EXPECT_TRUE(has_insert);
    EXPECT_EQ(1, pair->key());
    pair->set_value(100);

    pair = map.GetOrInsert(111, &has_insert);
    ASSERT_TRUE(pair != nullptr);
    EXPECT_TRUE(has_insert);
    EXPECT_EQ(111, pair->key());
    pair->set_value(200);
}

TEST(ZoneHashMapTest, InsertThenGet) {
    Zone zone;
    IntHashMap map(&zone);

    bool has_insert = false;
    auto pair = map.GetOrInsert(1, &has_insert);
    pair->set_value(100);

    pair = map.Get(1);
    ASSERT_TRUE(pair != nullptr);
    EXPECT_EQ(1, pair->key());
    EXPECT_EQ(100, pair->value());
}

TEST(ZoneHashMapTest, Rehash) {
    Zone zone;
    IntHashMap map(&zone);

    const auto N = IntHashMap::kDefaultNumberOfSlots * 3 + 2;
    for (int i = 0; i < N; ++i) {
        bool has_insert = false;
        auto pair = map.GetOrInsert(i, &has_insert);
        ASSERT_TRUE(has_insert);
        pair->set_value(i * 100 + 1);
    }
    EXPECT_EQ(N, map.size());
    EXPECT_EQ(IntHashMap::kDefaultNumberOfSlots * 2, map.num_slots());

    for (int i = 0; i < N; ++i) {
        auto pair = map.Get(i);
        ASSERT_TRUE(pair != nullptr);
        EXPECT_EQ(i, pair->key()) << "index: " << i;
        EXPECT_EQ(i * 100 + 1, pair->value()) << "index: " << i;
    }
}

} // namespace mio
