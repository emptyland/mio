#include "base.h"
#include "gtest/gtest.h"

namespace mio {

TEST(BaseTest, Saniy) {
    auto i = 1;
    ASSERT_EQ(1, i);
}

} // namespace mio
