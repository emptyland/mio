#include "asm-amd64.h"
#include "gtest/gtest.h"
#include <sys/mman.h>
#include <functional>
#include <set>

class AsmAmd64Test : public ::testing::Test {
public:
    AsmAmd64Test() : code_(nullptr) {
    }

    virtual void SetUp() {
        code_ = mmap(nullptr, kPageSize, PROT_READ|PROT_EXEC|PROT_WRITE,
                     MAP_ANON|MAP_PRIVATE, -1, 0);
        ASSERT_NE(nullptr, code_);
        memset(code_, 0xCC, kPageSize);

        asm_.pc = reinterpret_cast<uint8_t*>(code_);
        asm_.code = asm_.pc;
        asm_.size = kPageSize;
    }

    virtual void TearDown() {
        ASSERT_NE(nullptr, code_);
        auto rv = munmap(code_, kPageSize);
        ASSERT_EQ(0, rv);
    }


    void FunctionFrame(const std::function<void()> &body) {
        Emit_pushq_r(&asm_, rbp);
        Emit_movq_r_r(&asm_, rbp, rsp, kDefSize);

        body();

        Emit_popq_r(&asm_, rbp);
        Emit_ret_i(&asm_, 0);
    }

    template<class T>
    T *GetFunction(const std::function<void()> &body) {
        T *rv = GetFunction<T>(asm_.pc);
        FunctionFrame(body);
        return rv;
    }

    template<class T>
    T *GetFunction() {
        return GetFunction<T>(asm_.code);
    }

    template<class T>
    static T *GetFunction(void *raw) {
        return reinterpret_cast<T*>(static_cast<void*>(raw));
    }

    static const size_t kPageSize = 4096;
    static const int kI64Size = sizeof(uint64_t);
    static const int kI32Size = sizeof(uint32_t);
    static const int kDefSize = kI64Size;

    void *code_;
    Asm asm_;
};

TEST_F(AsmAmd64Test, OnlyReturn) {
    Emit_pushq_r(&asm_, rbp);
    Emit_movq_i64(&asm_, rax, 100);
    Emit_popq_r(&asm_, rbp);
    Emit_ret_i(&asm_, 0);

    typedef int64_t (*StubFnType)();

    auto fn = reinterpret_cast<StubFnType>(code_);
    ASSERT_EQ(100, fn());
}

TEST_F(AsmAmd64Test, ReturnArg1) {
    Emit_pushq_r(&asm_, rbp);
    Emit_movq_r_r(&asm_, rbp, rsp, kDefSize);
    Emit_movq_r_r(&asm_, rax, RegArgv[1], kDefSize);
    Emit_popq_r(&asm_, rbp);
    Emit_ret_i(&asm_, 0);

    typedef int64_t (*StubFnType)(int64_t, int64_t, int64_t, int64_t);
    auto fn = reinterpret_cast<StubFnType>(code_);
    ASSERT_EQ(1, fn(0, 1, 2, 3));
}

struct AccessStub {
    int8_t  _0;
    int64_t _1;
    int64_t _2;
};

intptr_t kOffset_0 = reinterpret_cast<intptr_t>(&reinterpret_cast<AccessStub*>(0)->_0);
intptr_t kOffset_1 = reinterpret_cast<intptr_t>(&reinterpret_cast<AccessStub*>(0)->_1);
intptr_t kOffset_2 = reinterpret_cast<intptr_t>(&reinterpret_cast<AccessStub*>(0)->_2);

TEST_F(AsmAmd64Test, StructAccess) {
    AccessStub stub{0, 0, 0};

    Emit_pushq_r(&asm_, rbp);
    Emit_movq_r_r(&asm_, rbp, rsp, kDefSize);

    Opd field;
    Emit_movq_i64(&asm_, rax, 9);
    Operand0(&field, RegArgv[0], static_cast<int32_t>(kOffset_1));
    Emit_add_op_r(&asm_, &field, rax, kDefSize);

    Emit_movq_i64(&asm_, rax, 8);
    Operand0(&field, RegArgv[0], static_cast<int32_t>(kOffset_2));
    Emit_add_op_r(&asm_, &field, rax, kDefSize);

    Emit_movq_r_r(&asm_, rax, RegArgv[0], kDefSize);
    Emit_popq_r(&asm_, rbp);
    Emit_ret_i(&asm_, 0);

    typedef AccessStub * (*StubFnType)(AccessStub *);
    auto fn = reinterpret_cast<StubFnType>(code_);
    fn(&stub);

    ASSERT_EQ(9, stub._1);
    ASSERT_EQ(8, stub._2);
}

TEST_F(AsmAmd64Test, ArrayAccess) {
    int64_t stub[16] = {0};

    Emit_pushq_r(&asm_, rbp);
    Emit_movq_r_r(&asm_, rbp, rsp, kDefSize);

    Emit_xor_r_r(&asm_, rcx, rcx, kDefSize);

    Opd ptr;
    Operand1(&ptr, RegArgv[0], rcx, times_8, 0);
    Emit_movq_op_i(&asm_, &ptr, Imm{111}, kDefSize);

    Emit_add_r_i(&asm_, rcx, Imm{1}, kDefSize);
    Emit_movq_op_i(&asm_, &ptr, Imm{222}, kDefSize);

    Emit_add_r_i(&asm_, rcx, Imm{1}, kDefSize);
    Emit_movq_op_i(&asm_, &ptr, Imm{333}, kDefSize);

    Emit_xor_r_r(&asm_, rax, rax, kDefSize);
    Emit_popq_r(&asm_, rbp);
    Emit_ret_i(&asm_, 0);

    typedef void (*StubFnType)(int64_t *);
    auto fn = reinterpret_cast<StubFnType>(code_);
    fn(stub);

    ASSERT_EQ(111L, stub[0]);
    ASSERT_EQ(222L, stub[1]);
    ASSERT_EQ(333L, stub[2]);
}

static int64_t CallStub(int64_t input) {
    printf("in asm: %lld\n", input);
    return input * 3;
}

TEST_F(AsmAmd64Test, Calling) {
    Emit_pushq_r(&asm_, rbp);
    Emit_movq_r_r(&asm_, rbp, rsp, kDefSize);
    Emit_subq_r_i(&asm_, rsp, Imm{kDefSize});

    Emit_movq_r_r(&asm_, rax, RegArgv[0], kDefSize);
    Emit_addq_r_i(&asm_, rax, Imm{7});
    Emit_movq_r_r(&asm_, RegArgv[0], rax, kDefSize);
    Emit_call_p(&asm_, reinterpret_cast<void *>(CallStub));

    //Emit_xor_r_r(&asm_, rax, rax, kDefSize);
    Emit_addq_r_i(&asm_, rsp, Imm{kDefSize});
    Emit_popq_r(&asm_, rbp);
    Emit_ret_i(&asm_, 0);

    typedef int64_t (*StubFnType)(int64_t);
    auto fn = reinterpret_cast<StubFnType>(code_);
    ASSERT_EQ(24, fn(1));
    ASSERT_EQ(27, fn(2));
    ASSERT_EQ(30, fn(3));
}

TEST_F(AsmAmd64Test, LabelAndJmp) {
    Emit_pushq_r(&asm_, rbp);
    Emit_movq_r_r(&asm_, rbp, rsp, kDefSize);

    YILabel l1 = YI_LABEL_INIT();
    Emit_jmp_l(&asm_, &l1, 1);

    Emit_int3(&asm_);
    Emit_int3(&asm_);
    Emit_int3(&asm_);

    Bind(&asm_, &l1);
    Emit_xor_r_r(&asm_, rax, rax, kDefSize);
    Emit_popq_r(&asm_, rbp);
    Emit_ret_i(&asm_, 0);

    typedef void (*StubFnType)();
    auto fn = reinterpret_cast<StubFnType>(code_);
    fn();
}

TEST_F(AsmAmd64Test, LabelAndCall) {
    YILabel foo = YI_LABEL_INIT();

    FunctionFrame([&] () {
        // function: bar
        Emit_movq_i64(&asm_, RegArgv[0], 100);
        Emit_movq_i64(&asm_, RegArgv[1], 200);
        Emit_call_l(&asm_, &foo);
    });

    // function: foo
    Bind(&asm_, &foo);
    FunctionFrame([&] () {
        Emit_add_r_r(&asm_, RegArgv[0], RegArgv[1], kDefSize);
        Emit_movq_r_r(&asm_, rax, RegArgv[0], kDefSize);
    });

    auto fn = GetFunction<int64_t()>();
    ASSERT_EQ(300, fn());
}

TEST_F(AsmAmd64Test, ConditionJump) {
    FunctionFrame([&] () {
        YILabel l_ge = YI_LABEL_INIT();

        Emit_cmp_r_r(&asm_, RegArgv[0], RegArgv[1], kDefSize);
        Emit_jcc_l(&asm_, GreaterEqual, &l_ge, 1);
        Emit_xor_r_r(&asm_, rax, rax, kDefSize);
        Emit_popq_r(&asm_, rbp);
        Emit_ret_i(&asm_, 0);

        Bind(&asm_, &l_ge);
        Emit_movq_i64(&asm_, rax, 1);
    });

    auto fn = GetFunction<int(int64_t, int64_t)>();
    ASSERT_EQ(1, fn(1, 1));
    ASSERT_EQ(1, fn(2, 1));
    ASSERT_EQ(0, fn(0, 1));
}

TEST_F(AsmAmd64Test, FloatLoadAndAdd) {
    FunctionFrame([&] () {
        Opd opd;

        Emit_movss_x_op(&asm_, xmm0, Operand0(&opd, RegArgv[0], 0));
        Emit_movss_x_op(&asm_, xmm1, Operand0(&opd, RegArgv[1], 0));

        Emit_movaps_x_x(&asm_, xmm8, xmm1);
        Emit_addss_x_x(&asm_, xmm0, xmm8);
    });

    auto fn = GetFunction<float(float*, float*)>();

    float op1 = 3.12f, op2 = 2.2f;
    ASSERT_EQ(op1 + op2, fn(&op1, &op2));
}

TEST_F(AsmAmd64Test, DoubleLoadAndAdd) {
    FunctionFrame([&] () {
        Opd opd;

        Emit_movsd_x_op(&asm_, xmm0, Operand0(&opd, RegArgv[0], 0));
        Emit_movsd_x_op(&asm_, xmm1, Operand0(&opd, RegArgv[1], 0));

        Emit_movapd_x_x(&asm_, xmm8, xmm1);
        Emit_addsd_x_x(&asm_, xmm0, xmm8);
    });

    auto fn = GetFunction<double(double*, double*)>();

    double op1 = 3.14f, op2 = 2.2888888f;
    ASSERT_EQ(op1 + op2, fn(&op1, &op2));
}

TEST_F(AsmAmd64Test, FloatSSEArith) {
    FunctionFrame([&] () {
        // 4 * 3 / 2 - 1 + 0
        Emit_mulss_x_x(&asm_, xmm3, xmm4);
        Emit_divss_x_x(&asm_, xmm2, xmm3);
        Emit_subss_x_x(&asm_, xmm1, xmm2);
        Emit_addss_x_x(&asm_, xmm0, xmm1);
    });

    auto fn = GetFunction<float(float, float, float, float, float)>();
    float op[5] = { 1.1, 2.3, 3.4, 5.6, 7.7 };
    auto rv = fn(op[0], op[1], op[2], op[3], op[4]);
    op[3] *= op[4];
    op[2] /= op[3];
    op[1] -= op[2];
    op[0] += op[1];
    ASSERT_EQ(rv, op[0]);
}

TEST_F(AsmAmd64Test, DoubleSSEArith) {
    FunctionFrame([&] () {
        // 4 * 3 / 2 - 1 + 0
        Emit_mulsd_x_x(&asm_, xmm3, xmm4);
        Emit_divsd_x_x(&asm_, xmm2, xmm3);
        Emit_subsd_x_x(&asm_, xmm1, xmm2);
        Emit_addsd_x_x(&asm_, xmm0, xmm1);
    });

    auto fn = GetFunction<double(double, double, double, double, double)>();
    double op[5] = { 1.1f, 2.3f, 3.4f, 5.6f, 7.7f };
    auto rv = fn(op[0], op[1], op[2], op[3], op[4]);
    op[3] *= op[4];
    op[2] /= op[3];
    op[1] -= op[2];
    op[0] += op[1];
    ASSERT_EQ(rv, op[0]);
}

TEST_F(AsmAmd64Test, FloatSIMDSanity) {
    FunctionFrame([&] () {
        Opd op;

        Emit_movaps_x_op(&asm_, xmm0, Operand0(&op, RegArgv[0], 0));
        Emit_addps_x_op(&asm_, xmm0, Operand0(&op, RegArgv[1], 0));
        Emit_movaps_op_x(&asm_, Operand0(&op, RegArgv[2], 0), xmm0);
    });

    float op1[4] = { 1.1f, 2.2f, 3.14f, 100.9f };
    float op2[4] = { 1.1f, 2.2f, 3.14f, 100.9f };
    float rv[4] = {0};

    auto fn = GetFunction<void(float *, float *, float *)>();
    fn(op1, op2, rv);
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(rv[i], op1[i] + op2[i]);
    }
}

TEST_F(AsmAmd64Test, FloatCompare) {
    FunctionFrame([&] () {
        Emit_ucomiss_x_x(&asm_, xmm0, xmm1);

        YILabel then = YI_LABEL_INIT();
        Emit_jcc_l(&asm_, Below, &then, 1);
        Emit_xor_r_r(&asm_, rax, rax, kDefSize);
        Emit_popq_r(&asm_, rbp);
        Emit_ret_i(&asm_, 0);

        Bind(&asm_, &then);
        Emit_movq_r_i(&asm_, rax, Imm{1}, sizeof(int32_t));
    });

    auto fn = GetFunction<int(float, float)>();
    ASSERT_EQ(1, fn(-1, 1));
    ASSERT_EQ(0, fn(1, -1));
    ASSERT_EQ(0, fn(1, 1));
}

TEST_F(AsmAmd64Test, DoubleSIMDSanity) {
    FunctionFrame([&] () {
        Opd op;

        Emit_movapd_x_op(&asm_, xmm0, Operand0(&op, RegArgv[0], 0));
        Emit_addpd_x_op(&asm_, xmm0, Operand0(&op, RegArgv[1], 0));
        Emit_movapd_op_x(&asm_, Operand0(&op, RegArgv[2], 0), xmm0);
    });

    double op1[2] = { 3.14f, 100.9f };
    double op2[2] = { 3.14f, 100.9f };
    double rv[2] = {0};

    auto fn = GetFunction<void(double *, double *, double *)>();
    fn(op1, op2, rv);
    for (int i = 0; i < 2; ++i) {
        ASSERT_EQ(rv[i], op1[i] + op2[i]);
    }
}

TEST_F(AsmAmd64Test, FloatToDWord) {
    FunctionFrame([&] () {
        Emit_cvttss2si_r_x(&asm_, rax, xmm0, kI32Size);
    });

    auto fn = GetFunction<int(float)>();
    ASSERT_EQ(0, fn(0.001));
    ASSERT_EQ(1, fn(1.001));
}

TEST_F(AsmAmd64Test, FloatToDWord2) {
    FunctionFrame([&] () {
        Opd op;

        Emit_cvttss2si_r_op(&asm_, rax, Operand0(&op, RegArgv[0], 0), kI32Size);
    });

    auto fn = GetFunction<int(float *)>();
    float op1 = 1.0001, op2 = 0.0002;
    ASSERT_EQ(1, fn(&op1));
    ASSERT_EQ(0, fn(&op2));
}

TEST_F(AsmAmd64Test, ByteSignExtend) {
    auto fn_sx = GetFunction<int(int8_t)>([&] () {
        Emit_movq_r_r(&asm_, rbx, RegArgv[0], kDefSize);
        Emit_movsxb_r_r(&asm_, rax, rbx);
    });

    ASSERT_EQ(-1, fn_sx(-1));
    ASSERT_EQ(0, fn_sx(0));
    ASSERT_EQ(1, fn_sx(1));

    auto fn_zx = GetFunction<int(int8_t)>([&] () {
        Emit_movq_r_r(&asm_, rbx, RegArgv[0], kDefSize);
        Emit_movzxb_r_r(&asm_, rax, rbx);
    });

    ASSERT_EQ(255, fn_zx(-1));
    ASSERT_EQ(0, fn_zx(0));
    ASSERT_EQ(1, fn_zx(1));
}

TEST_F(AsmAmd64Test, ReadRandom) {
    auto rand32 = GetFunction<int32_t()>([&] () {
        Emit_rdrand(&asm_, rax, kI32Size);
    });

    std::set<int32_t> unique_i32;
    int i = 1000;
    while (i--)
        unique_i32.insert(rand32());
    EXPECT_NE(1, unique_i32.size());

    auto rand64 = GetFunction<int64_t()>([&] () {
        Emit_rdrand(&asm_, rax, kI64Size);
    });

    std::set<int64_t> unique_i64;
    i = 1000;
    while (i--)
        unique_i64.insert(rand64());
    EXPECT_NE(1, unique_i64.size());
}

TEST_F(AsmAmd64Test, Int8Movement) {
    auto fn = GetFunction<int8_t()>([&] () {
        Emit_xor_r_r(&asm_, rax, rax, kDefSize);
        Emit_movb_r_i(&asm_, rbx, Imm{100});
        Emit_movb_r_r(&asm_, rax, rbx);
        Emit_movb_r_i(&asm_, r15, Imm{200});
        Emit_movb_r_r(&asm_, r15, rax);
    });
    auto rv = fn();
    ASSERT_EQ(100, rv);
}

TEST_F(AsmAmd64Test, Int16Movement) {
    auto fn = GetFunction<int8_t()>([&] () {
        Emit_xor_r_r(&asm_, rax, rax, kDefSize);
        Emit_movw_r_i(&asm_, rbx, Imm{100});
        Emit_movw_r_r(&asm_, rax, rbx);
        Emit_movw_r_i(&asm_, r15, Imm{200});
        Emit_movw_r_r(&asm_, r15, rax);
    });
    auto rv = fn();
    ASSERT_EQ(100, rv);
}
