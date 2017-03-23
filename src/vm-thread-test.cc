#include "vm-thread.h"
#include "vm-stack.h"
#include "vm-bitcode-builder.h"
#include "vm-bitcode-disassembler.h"
#include "vm-objects.h"
#include "vm.h"
#include "handles.h"
#include "code-label.h"
#include "gtest/gtest.h"

namespace mio {

class ThreadTest : public ::testing::Test {
public:

    virtual void SetUp() override {
        vm_ = new VM();
    }

    virtual void TearDown() override {
        delete vm_;
    }

protected:
    VM *vm_ = nullptr;
};

TEST_F(ThreadTest, Sanity) {
    BitCodeBuilder builder(vm_->TEST_code());

    builder.frame(8, 0);
    builder.load_i32_imm(0, 11);
    builder.load_i32_imm(4, 22);
    builder.add_i32_imm(0, 4, 33);
    builder.ret();

    bool ok = true;
    vm_->TEST_main_thread()->Execute(0, &ok);
    ASSERT_TRUE(ok) << vm_->TEST_main_thread()->exit_code();
    ASSERT_EQ(Thread::SUCCESS, vm_->TEST_main_thread()->exit_code());
}

int FooRoutine(VM *vm, Thread *thread) {
    printf("call in\n");
    return 0;
}

TEST_F(ThreadTest, CallNativeFunction) {
    BitCodeBuilder builder(vm_->TEST_code());

    builder.frame(0, 8);
    builder.call_val(0, 0, 0);
    builder.ret();

    auto nfn = vm_->CreateNativeFunction("foo", FooRoutine);
    vm_->TEST_main_thread()->o_stack()->Push(nfn.get());
    ASSERT_EQ(nfn.get(), vm_->TEST_main_thread()->o_stack()->Get<MIONativeFunction*>(0));
    ASSERT_EQ(HeapObject::kNativeFunction, nfn->GetKind());

    bool ok = true;
    vm_->TEST_main_thread()->Execute(0, &ok);
    ASSERT_TRUE(ok) << vm_->TEST_main_thread()->exit_code();
    ASSERT_EQ(Thread::SUCCESS, vm_->TEST_main_thread()->exit_code());
}

} // namespace mio
