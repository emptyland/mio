#include "zone-vector.h"
#include "gtest/gtest.h"

namespace mio {

typedef ZoneVector<int> IntVector;

TEST(ZoneVectorTest, Sanity) {
    Zone zone;
    IntVector vector(&zone);

    EXPECT_EQ(0, vector.size());
    EXPECT_EQ(0, vector.capacity());

    vector.Add(0);
    vector.Add(1);
    vector.Add(2);

    EXPECT_EQ(3, vector.size());
    EXPECT_EQ(IntVector::kDefaultCapacity, vector.capacity());
}

TEST(ZoneVectorTest, Add) {
    Zone zone;
    IntVector vector(&zone);

    vector.Add(1);
    vector.Add(100);
    vector.Add(111);

    EXPECT_EQ(3, vector.size());
    EXPECT_EQ(1, vector.At(0));
    EXPECT_EQ(111, vector.At(2));
    EXPECT_EQ(100, vector.At(1));
}

TEST(ZoneVectorTest, Set) {
    Zone zone;
    IntVector vector(&zone);

    vector.Resize(4);
    vector.Set(0, 1000);
    vector.Set(3, 1001);

    EXPECT_EQ(4, vector.size());
    EXPECT_EQ(8, vector.capacity());
    EXPECT_EQ(1000, vector.At(0));
    EXPECT_EQ(1001, vector.At(3));
}

} // namespace mio
