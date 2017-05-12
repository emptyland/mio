#include "vm-thread.h"
#include "vm-stack.h"
#include "vm-bitcode-builder.h"
#include "vm-bitcode-disassembler.h"
#include "vm-objects.h"
#include "vm.h"
#include "vm-function-register.h"
#include "handles.h"
#include "code-label.h"
#include "gtest/gtest.h"

namespace mio {

class ThreadTest : public ::testing::Test {
public:

    virtual void SetUp() override {
        vm_ = new VM();
        vm_->AddSerachPath("libs");
        ASSERT_TRUE(vm_->Init());
    }

    virtual void TearDown() override {
        delete vm_;
    }

protected:
    VM *vm_ = nullptr;
};

int PrintRountine(VM *vm, Thread *thread) {
    auto ob = thread->GetObject(0);

    if (!ob->IsString()) {
        printf("error: parameter is not string\n");
    } else {
        printf("[%p] %s", ob.get(), ob->AsString()->GetData());
    }
    return 0;
}

TEST_F(ThreadTest, P012_Sanity) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/012", &error)) << error.ToString();
    vm_->function_register()->RegisterNativeFunction("::main::print", PrintRountine);
    ASSERT_EQ(0, vm_->Run());
}

TEST_F(ThreadTest, P013_UnionOperation) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/013", &error)) << error.ToString();

    std::string buf;
    vm_->function_register()->RegisterNativeFunction("::main::print", PrintRountine);
    if (vm_->Run() != 0) {
        buf.clear();
        vm_->PrintBackstrace(&buf);
        printf("%s\n", buf.c_str());
    }
}

TEST_F(ThreadTest, P014_LocalFunction) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/014", &error)) << error.ToString();

    std::string buf;
    vm_->function_register()->RegisterNativeFunction("::main::print", PrintRountine);
    if (vm_->Run() != 0) {
        buf.clear();
        vm_->PrintBackstrace(&buf);
        printf("%s\n", buf.c_str());
    }
}

TEST_F(ThreadTest, P015_HashMapForeach) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/015", &error)) << error.ToString();
    vm_->function_register()->RegisterNativeFunction("::main::print", PrintRountine);
    ASSERT_EQ(0, vm_->Run());
}

TEST_F(ThreadTest, P016_UnionTypeMatch) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/016", &error)) << error.ToString();
    vm_->function_register()->RegisterNativeFunction("::main::print", PrintRountine);
    ASSERT_EQ(0, vm_->Run());
}

TEST_F(ThreadTest, P017_PanicTest) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/017", &error)) << error.ToString();
    std::string buf;
    if (vm_->Run() != 0) {
        buf.clear();
        vm_->PrintBackstrace(&buf);
        printf("%s\n", buf.c_str());
    }
}

TEST_F(ThreadTest, P018_ArrayInitializerAndForeach) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/018", &error)) << error.ToString();

    std::string buf;
    vm_->function_register()->RegisterNativeFunction("::main::print", PrintRountine);
    if (vm_->Run() != 0) {
        buf.clear();
        vm_->PrintBackstrace(&buf);
        printf("%s\n", buf.c_str());
    }
}

TEST_F(ThreadTest, P020_ErrorType) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/020", &error)) << error.ToString();
    std::string buf;

    if (vm_->Run() != 0) {
        buf.clear();
        vm_->PrintBackstrace(&buf);
        printf("%s\n", buf.c_str());
    }
}

TEST_F(ThreadTest, P021_LenBuiltinCall) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/021", &error)) << error.ToString();
    std::string buf;
    vm_->DisassembleAll(&buf);
    printf("%s\n", buf.c_str());

    vm_->function_register()->RegisterNativeFunction("::main::print", PrintRountine);
    if (vm_->Run() != 0) {
        buf.clear();
        vm_->PrintBackstrace(&buf);
        printf("%s\n", buf.c_str());
    }
}

} // namespace mio
