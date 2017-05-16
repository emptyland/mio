#include "base.h"
#include "gtest/gtest.h"
#include "glog/logging.h"

int main(int argc, char *argv[]) {
    google::InitGoogleLogging(argv[0]);
    ::mio::EnvirmentInitialize();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
