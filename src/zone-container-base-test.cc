#include "zone-container-base.h"
#include "gtest/gtest.h"

namespace mio {

class ZoneLinkedArrayTest : public ::testing::Test {
public:
    virtual void SetUp() override {
        zone_ = new Zone;
    }

    virtual void TearDown() override {
        delete zone_;
    }

protected:
    Zone *zone_;
};

TEST_F(ZoneLinkedArrayTest, Sanity) {
    ZoneLinkedArray<int> array(zone_);

    ASSERT_EQ(0, array.size());
    ASSERT_EQ(ZoneLinkedArray<int>::kDefaultCapacity, array.capacity());
    ASSERT_EQ(251, array.segment_max_capacity());
}

TEST_F(ZoneLinkedArrayTest, Pointer) {
    ZoneLinkedArray<void *> array(zone_);

    ASSERT_EQ(0, array.size());
    ASSERT_EQ(ZoneLinkedArray<void *>::kDefaultCapacity, array.capacity());
    ASSERT_EQ(251, array.segment_max_capacity());
}

TEST_F(ZoneLinkedArrayTest, Advance) {
    ZoneLinkedArray<int> array(zone_);

    for (int i = 0; i < ZoneLinkedArray<int>::kDefaultCapacity + 1; ++i) {
        array.Add(i);
    }
    ASSERT_EQ(ZoneLinkedArray<int>::kDefaultCapacity * 2, array.capacity());

    for (int i = 0; i < ZoneLinkedArray<int>::kDefaultCapacity + 1; ++i) {
        ASSERT_EQ(i, array.Get(i));
    }
}

} // namespace mio
