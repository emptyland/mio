#include "gtest/gtest.h"
#include <atomic>
#include <thread>

namespace mio {

TEST(HandlesTest, AtomicOps) {
    std::atomic<uint32_t> val(1);

    uint32_t nval = 1;
    ASSERT_TRUE(val.compare_exchange_strong(nval, 0));
    ASSERT_EQ(0, val.load());
    ASSERT_EQ(1, nval);

    ASSERT_FALSE(val.compare_exchange_strong(nval, 1));
    nval = 0;
    ASSERT_TRUE(val.compare_exchange_strong(nval, 1));
}

} // namespace mio
