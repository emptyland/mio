#include "vm-thread.h"
#include "vm-stack.h"
#include "vm-bitcode-builder.h"
#include "vm-bitcode-disassembler.h"
#include "vm-objects.h"
#include "vm.h"
#include "vm-function-register.h"
#include "malloced-object-factory.h"
#include "handles.h"
#include "code-label.h"
#include "gtest/gtest.h"

namespace mio {

class ThreadTest : public ::testing::Test {
public:

    virtual void SetUp() override {
        vm_ = new VM();
        ASSERT_TRUE(vm_->Init());
        factory_ = new MallocedObjectFactory;
    }

    virtual void TearDown() override {
        delete factory_;
        delete vm_;
    }

protected:
    VM *vm_ = nullptr;
    MallocedObjectFactory *factory_ = nullptr;
};

int PrintRountine(VM *vm, Thread *thread) {
    auto ob = thread->GetObject(0);

    printf("%s", ob->AsString()->mutable_data());
    return 0;
}

TEST_F(ThreadTest, P012_Sanity) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/012", &error)) << error.ToString();
    std::string buf;
    vm_->DisassembleAll(&buf);

    printf("%s\n", buf.c_str());

    vm_->function_register()->RegisterNativeFunction("::main::print", PrintRountine);
    ASSERT_EQ(0, vm_->Run());
}

} // namespace mio
