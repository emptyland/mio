#include "vm-thread.h"
#include "vm-stack.h"
#include "vm-bitcode-builder.h"
#include "vm-bitcode-disassembler.h"
#include "vm-object-factory.h"
#include "vm-object-surface.h"
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

mio_i64_t TestNativeFoo1(Thread *t) {
    return t->vm()->tick();
}

mio_f32_t TestNativeFoo2(Thread *t) {
    return 1.101f;
}

mio_i64_t TestNativeFoo3(Thread *t, mio_i64_t add) {
    return 100 + add;
}

mio_f64_t TestNativeFoo4(Thread *t, mio_i64_t a, mio_f64_t b) {
    return a + b;
}

mio_i64_t TestNativeFoo5(Thread *t, MIOString *s) {
    return s->GetLength();
}

MIOString *TestNativeFoo6(Thread *t, mio_int_t a) {
    char buf[64];
    snprintf(buf, arraysize(buf), "--to--: %lld", a);
    return t->vm()->object_factory()->GetOrNewString(buf).get();
}

MIOExternal *TestNativeFoo7(Thread *t) {
    return t->vm()->object_factory()->NewExternalTemplate(new std::string("ok")).get();
}

void TestNativeFoo8(Thread *t, MIOExternal *ex) {
    auto s = MIOExternalStub::Get<std::string>(ex);
    printf("extenal: %s\n", s->c_str());
    delete s;
    ex->SetValue(nullptr);
}

TEST_F(ThreadTest, P022_FunctionTemplate) {
    ParsingError error;

    ASSERT_TRUE(vm_->CompileProject("test/022", &error)) << error.ToString();
    auto ok = vm_->function_register()->RegisterFunctionTemplate("::main::foo1",
                                                                 &TestNativeFoo1);
    ASSERT_TRUE(ok);
    ok = vm_->function_register()->RegisterFunctionTemplate("::main::foo2",
                                                            &TestNativeFoo2);
    ASSERT_TRUE(ok);
    ok = vm_->function_register()->RegisterFunctionTemplate("::main::foo3",
                                                            &TestNativeFoo3);
    ASSERT_TRUE(ok);
    ok = vm_->function_register()->RegisterFunctionTemplate("::main::foo4",
                                                            &TestNativeFoo4);
    ASSERT_TRUE(ok);
    ok = vm_->function_register()->RegisterFunctionTemplate("::main::foo5",
                                                            &TestNativeFoo5);
    ASSERT_TRUE(ok);
    ok = vm_->function_register()->RegisterFunctionTemplate("::main::foo6",
                                                            &TestNativeFoo6);
    ASSERT_TRUE(ok);
    ok = vm_->function_register()->RegisterFunctionTemplate("::main::foo7",
                                                            &TestNativeFoo7);
    ASSERT_TRUE(ok);
    ok = vm_->function_register()->RegisterFunctionTemplate("::main::foo8",
                                                            &TestNativeFoo8);
    ASSERT_TRUE(ok);

    if (vm_->Run() != 0) {
        std::string buf;
        vm_->PrintBackstrace(&buf);
        printf("%s\n", buf.c_str());
    }
}

} // namespace mio
