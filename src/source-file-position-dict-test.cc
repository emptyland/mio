#include "source-file-position-dict.h"
#include "gtest/gtest.h"

namespace mio {

TEST(SourceFilePositionDictTest, Sanity) {
    SourceFilePositionDict dict;
    bool ok = true;
    auto line = dict.GetLine("test/line-test.txt", 0, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(0, line.line);
    EXPECT_EQ(0, line.column);

    line = dict.GetLine("test/line-test.txt", 15, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(0, line.line);
    EXPECT_EQ(15, line.column);

    line = dict.GetLine("test/line-test.txt", 16, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(0, line.line);
    EXPECT_EQ(16, line.column);

    line = dict.GetLine("test/line-test.txt", 17, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(1, line.line);
    EXPECT_EQ(0, line.column);

    line = dict.GetLine("test/line-test.txt", 18, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(1, line.line);
    EXPECT_EQ(1, line.column);
}

} // namespace mio
