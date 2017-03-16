#include "vm-memory-segment.h"
#include "gtest/gtest.h"

namespace mio {

TEST(MemorySegmentTest, Sanity) {
    MemorySegment seg;

    ASSERT_EQ(4096, seg.capacity());
    ASSERT_EQ(0, seg.size());
}

TEST(MemorySegmentTest, Advance) {
    MemorySegment seg;

    auto base = static_cast<uint8_t *>(seg.base());
    auto p = static_cast<uint8_t *>(seg.Advance(3));
    EXPECT_EQ(0, p - base);
    EXPECT_EQ(3, seg.size());

    p = static_cast<uint8_t *>(seg.AlignAdvance(1));
    EXPECT_EQ(7, seg.size());
}

} // namespace mio
