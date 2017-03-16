#include "vm-bitcode-builder.h"
#include "vm-bitcode-disassembler.h"
#include "code-label.h"
#include "gtest/gtest.h"

namespace mio {

TEST(BitCodeBuilderTest, Sanity) {
    MemorySegment code;
    BitCodeBuilder builder(&code);

    ASSERT_EQ(0, builder.pc());
    builder.debug();
    builder.debug();
    ASSERT_EQ(2, builder.pc());

    CodeLabel label;

    ASSERT_TRUE(label.is_unused());
    builder.BindTo(&label, 1);
    ASSERT_TRUE(label.is_bound());
    EXPECT_EQ(1, label.position());
}

TEST(BitCodeBuilderTest, LabelBindThenUse) {
    MemorySegment code;
    BitCodeBuilder builder(&code);

    builder.debug(); // [0]
    builder.debug(); // [1]

    CodeLabel label;
    builder.BindTo(&label, 0);
    builder.load(&label); // [2]

    EXPECT_EQ(3, builder.pc());

    auto off = *static_cast<int32_t *>(code.offset(2 * 8));
    ASSERT_EQ(-2, off);
}

TEST(BitCodeBuilderTest, LabelLinkThenUse) {
    MemorySegment code;
    BitCodeBuilder builder(&code);

    builder.debug(); // [0]
    builder.debug(); // [1]

    CodeLabel label;
    builder.load(&label); // [2]
    builder.BindTo(&label, 0);

    EXPECT_EQ(3, builder.pc());

    auto off = *static_cast<int32_t *>(code.offset(2 * 8));
    ASSERT_EQ(-2, off);
}

TEST(BitCodeBuilderTest, InstructionStruct) {
    auto bc = BitCodeBuilder::Make3AddrBC(BC_load_i32_imm, 100, 122, 233);

    EXPECT_EQ(BC_load_i32_imm, BitCodeDisassembler::GetInst(bc));
    EXPECT_EQ(100, BitCodeDisassembler::GetOp1(bc));
    EXPECT_EQ(122, BitCodeDisassembler::GetOp2(bc));
    EXPECT_EQ(233, BitCodeDisassembler::GetImm32(bc));
}

} // namespace mio
