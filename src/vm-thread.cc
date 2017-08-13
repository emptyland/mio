#include "vm-thread.h"
#include "vm-garbage-collector.h"
#include "vm-objects.h"
#include "vm-object-surface.h"
#include "vm-stack.h"
#include "vm-memory-segment.h"
#include "vm-bitcode-disassembler.h"
#include "vm-bitcode.h"
#include "vm.h"
#include "tracing.h"
#include "memory-output-stream.h"
#include "handles.h"
#include "glog/logging.h"
#include <float.h>
#include <limits>

namespace mio {

struct CallContext {
    int p_stack_base;
    int p_stack_size;
    int o_stack_base;
    int o_stack_size;

    MIOFunction *callee;

    int pc;
    uint64_t *bc;

    inline MIOGeneratedFunction *generated_function() {
        auto fn = callee->AsGeneratedFunction();
        return fn ? fn : DCHECK_NOTNULL(callee->AsClosure())->GetFunction()->AsGeneratedFunction();
    }

    inline mio_buf_t<uint8_t> const_primitive_buf() {
        return DCHECK_NOTNULL(generated_function())->GetConstantPrimitiveBuf();
    }

    inline mio_buf_t<HeapObject *> const_object_buf() {
        return DCHECK_NOTNULL(generated_function())->GetConstantObjectBuf();
    }

    inline mio_buf_t<UpValDesc> upvalue_buf() {
        auto closure = DCHECK_NOTNULL(callee->AsClosure());
        DCHECK(closure->IsClose());
        return closure->GetUpValuesBuf();
    }

    inline FunctionDebugInfo *debug_info() {
        return DCHECK_NOTNULL(generated_function())->GetDebugInfo();
    }
};

class CallStack {
public:
    static const int kSizeofElem = sizeof(CallContext);

    CallStack(int max_deep)
        : max_deep_(max_deep)
        , core_(new CallContext[max_deep])
        , top_(core_) {
    }

    ~CallStack() { delete[] core_; }

    int size() const { return static_cast<int>(top_ - core_); }

    CallContext *base() const { return core_; }

    CallContext *Push() { return top_++; }

    CallContext *Top() { DCHECK_GT(size(), 0); return top_ - 1; }

    void Pop() {
        --top_;
        DCHECK_GE(top_, core_);
    }

private:
    CallContext *core_;
    CallContext *top_;
    int          max_deep_;
}; // class CallStack

#define RunGC() (vm_->gc_->Step(vm_->tick_))

#define TRACE(body) if (!(body)) { \
    Panic(OUT_OF_MEMORY, ok, "trace fail: out of memory."); \
    return; \
} (void)0

Thread::Thread(VM *vm)
    : vm_(DCHECK_NOTNULL(vm))
    , p_stack_(new Stack())
    , o_stack_(new Stack())
    , call_stack_(new CallStack(vm->max_call_deep())) {
}

Thread::~Thread() {
    delete p_stack_;
    delete o_stack_;
    delete call_stack_;
}

void Thread::Execute(MIOGeneratedFunction *callee, bool *ok) {
    auto init = call_stack_->Push();

    init->p_stack_base = p_stack_->base_size(),
    init->p_stack_size = p_stack_->size(),
    init->o_stack_base = o_stack_->base_size(),
    init->o_stack_size = o_stack_->size(),
    init->bc = static_cast<uint64_t *>(callee->GetCode());
    init->pc = 0;
    init->callee = nullptr;

    pc_ = init->pc;
    bc_ = init->bc;
    callee_ = callee;

    while (!should_exit_) {
        auto bc = bc_[pc_++];

        switch (BitCodeDisassembler::GetInst(bc)) {
            case BC_debug:
                *ok = false;
                exit_code_ = DEBUGGING;
                return;

        #define DEFINE_CASE(byte, bit) \
            case BC_load_##byte##b: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto segment = BitCodeDisassembler::GetOp2(bc); \
                auto offset = BitCodeDisassembler::GetImm32(bc); \
                ProcessLoadPrimitive(byte, dest, segment, offset, ok); \
                if (!*ok) { \
                    return; \
                } \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

            case BC_load_o: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto segment = BitCodeDisassembler::GetOp2(bc);
                auto offset = BitCodeDisassembler::GetImm32(bc);
                ProcessLoadObject(dest, segment, offset, ok);
                if (!*ok) {
                    return;
                }
            } break;

        #define DEFINE_CASE(byte, bit) \
            case BC_load_i##bit##_imm: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto imm32 = BitCodeDisassembler::GetImm32(bc); \
                p_stack_->Set(dest, static_cast<mio_i##bit##_t>(imm32)); \
            } break;
            MIO_SMI_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_mov_##byte##b: { \
                auto dest = BitCodeDisassembler::GetVal1(bc); \
                auto src  = BitCodeDisassembler::GetVal2(bc); \
                memcpy(p_stack_->offset(dest), p_stack_->offset(src), byte); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

            case BC_mov_o: {
                auto dest = BitCodeDisassembler::GetVal1(bc);
                auto src  = BitCodeDisassembler::GetVal2(bc);

                auto ob = o_stack_->Get<HeapObject *>(src);
                o_stack_->Set(dest, ob);
            } break;

        #define DEFINE_CASE(byte, bit) \
            case BC_and_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetI##bit(lhs) & GetI##bit(rhs)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_or_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetI##bit(lhs) | GetI##bit(rhs)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_xor_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetI##bit(lhs) ^ GetI##bit(rhs)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_inv_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto operand = BitCodeDisassembler::GetOp2(bc); \
                p_stack_->Set(dest, ~GetI##bit(operand)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_shl_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                if (GetInt(rhs) < 0) { \
                    Panic(PANIC, ok, "negative shift left: %lld", GetInt(rhs)); \
                    return; \
                } \
                p_stack_->Set<mio_i##bit##_t>(dest, GetI##bit(lhs) << GetInt(rhs)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_shr_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                if (GetInt(rhs) < 0) { \
                    Panic(PANIC, ok, "negative arithmetic shift right: %lld", GetInt(rhs)); \
                    return; \
                } \
                p_stack_->Set<mio_i##bit##_t>(dest, GetI##bit(lhs) >> GetInt(rhs)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_ushr_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                if (GetInt(rhs) < 0) { \
                    Panic(PANIC, ok, "negative logic shift right: %lld", GetInt(rhs)); \
                    return; \
                } \
                auto result = GetI##bit(lhs) >> GetInt(rhs); \
                p_stack_->Set<mio_i##bit##_t>(dest, GetI##bit(lhs) >= 0 ? result : result & ~(1ULL << ((bit) - 1))); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_shl_i##bit##_imm: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto imm = BitCodeDisassembler::GetImm32(bc); \
                if (imm < 0) { \
                    Panic(PANIC, ok, "negative shift left: %d", imm); \
                    return; \
                } \
                p_stack_->Set<mio_i##bit##_t>(dest, GetI##bit(lhs) << imm); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_shr_i##bit##_imm: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto imm = BitCodeDisassembler::GetImm32(bc); \
                if (imm < 0) { \
                    Panic(PANIC, ok, "negative shift left: %d", imm); \
                    return; \
                } \
                p_stack_->Set<mio_i##bit##_t>(dest, GetI##bit(lhs) >> imm); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_ushr_i##bit##_imm: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto imm = BitCodeDisassembler::GetImm32(bc); \
                if (imm < 0) { \
                    Panic(PANIC, ok, "negative shift left: %d", imm); \
                    return; \
                } \
                auto result = GetI##bit(lhs) >> imm; \
                p_stack_->Set<mio_i##bit##_t>(dest, GetI##bit(lhs) >= 0 ? result : result & ~(1ULL << ((bit) - 1))); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_add_i##bit##_imm: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs  = BitCodeDisassembler::GetOp2(bc); \
                auto imm32 = BitCodeDisassembler::GetImm32(bc); \
                p_stack_->Set(dest, GetI##bit(lhs) + static_cast<mio_i##bit##_t>(imm32)); \
            } break;
            MIO_SMI_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_add_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetI##bit(lhs) + GetI##bit(rhs)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_sub_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetI##bit(lhs) - GetI##bit(rhs)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_mul_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetI##bit(lhs) * GetI##bit(rhs)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_div_i##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                if (GetI##bit(rhs) == 0) { \
                    Panic(DIV_ZERO, ok, "div zero."); \
                    return; \
                } \
                p_stack_->Set(dest, GetI##bit(lhs) / GetI##bit(rhs)); \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_add_f##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetF##bit(lhs) + GetF##bit(rhs)); \
            } break;
            MIO_FLOAT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_sub_f##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetF##bit(lhs) - GetF##bit(rhs)); \
            } break;
            MIO_FLOAT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_mul_f##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetF##bit(lhs) * GetF##bit(rhs)); \
            } break;
            MIO_FLOAT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

        #define DEFINE_CASE(byte, bit) \
            case BC_div_f##bit: { \
                auto dest = BitCodeDisassembler::GetOp1(bc); \
                auto lhs = BitCodeDisassembler::GetOp2(bc); \
                auto rhs = BitCodeDisassembler::GetOp3(bc); \
                p_stack_->Set(dest, GetF##bit(lhs) / GetF##bit(rhs)); \
            } break;
            MIO_FLOAT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

            case BC_logic_not: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto operand = BitCodeDisassembler::GetOp2(bc);
                p_stack_->Set<mio_bool_t>(dest, GetI8(operand) == 0 ? 1 : 0);
            } break;

        #define DEFINE_CASE(byte, bit) \
            case BC_store_##byte##b: { \
                auto src = BitCodeDisassembler::GetOp1(bc); \
                auto segment = BitCodeDisassembler::GetOp2(bc); \
                auto dest = BitCodeDisassembler::GetImm32(bc); \
                ProcessStorePrimitive(byte, src, segment, dest, ok); \
                if (!*ok) { \
                    return; \
                } \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

            case BC_store_o: {
                auto src = BitCodeDisassembler::GetOp1(bc);
                auto segment = BitCodeDisassembler::GetOp2(bc);
                auto dest = BitCodeDisassembler::GetImm32(bc);
                ProcessStoreObject(src, segment, dest, ok);
                if (!*ok) {
                    return;
                }
            } break;

        #define DEFINE_CASE(byte, bit) \
            case BC_cmp_i##bit: { \
                auto op = static_cast<BCComparator>(BitCodeDisassembler::GetOp1(bc)); \
                auto result = BitCodeDisassembler::GetOp2(bc); \
                auto val1 = BitCodeDisassembler::GetVal1(bc); \
                auto val2 = BitCodeDisassembler::GetVal2(bc); \
                switch (op) { \
                    case CC_EQ: \
                        p_stack_->Set<mio_bool_t>(result, GetI##bit(val1) == GetI##bit(val2)); \
                        break; \
                    case CC_NE: \
                        p_stack_->Set<mio_bool_t>(result, GetI##bit(val1) != GetI##bit(val2)); \
                        break; \
                    case CC_LT: \
                        p_stack_->Set<mio_bool_t>(result, GetI##bit(val1) < GetI##bit(val2)); \
                        break; \
                    case CC_LE: \
                        p_stack_->Set<mio_bool_t>(result, GetI##bit(val1) <= GetI##bit(val2)); \
                        break; \
                    case CC_GT: \
                        p_stack_->Set<mio_bool_t>(result, GetI##bit(val1) > GetI##bit(val2)); \
                        break; \
                    case CC_GE: \
                        p_stack_->Set<mio_bool_t>(result, GetI##bit(val1) >= GetI##bit(val2)); \
                        break; \
                    default: \
                        Panic(PANIC, ok, "bad comparator %d", op); \
                        return; \
                } \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

            case BC_cmp_f32: {
                auto op = static_cast<BCComparator>(BitCodeDisassembler::GetOp1(bc));
                auto result = BitCodeDisassembler::GetOp2(bc);
                auto val1 = BitCodeDisassembler::GetVal1(bc);
                auto val2 = BitCodeDisassembler::GetVal2(bc);
                switch (op) {
                    case CC_EQ:
                        p_stack_->Set<mio_bool_t>(result, ::fabsf(GetF32(val1) - GetF32(val2)) < FLT_EPSILON);
                        break;
                    case CC_NE:
                        p_stack_->Set<mio_bool_t>(result, ::fabsf(GetF32(val1) - GetF32(val2)) >= FLT_EPSILON);
                        break;
                    case CC_LT:
                        p_stack_->Set<mio_bool_t>(result, GetF32(val1) < GetF32(val2));
                        break;
                    case CC_LE:
                        p_stack_->Set<mio_bool_t>(result, GetF32(val1) <= GetF32(val2));
                        break;
                    case CC_GT:
                        p_stack_->Set<mio_bool_t>(result, GetF32(val1) > GetF32(val2));
                        break;
                    case CC_GE:
                        p_stack_->Set<mio_bool_t>(result, GetF32(val1) >= GetF32(val2));
                        break;
                    default:
                        Panic(PANIC, ok, "bad comparator %d", op);
                        return;
                }
            } break;

            case BC_cmp_f64: {
                auto op = static_cast<BCComparator>(BitCodeDisassembler::GetOp1(bc));
                auto result = BitCodeDisassembler::GetOp2(bc);
                auto val1 = BitCodeDisassembler::GetVal1(bc);
                auto val2 = BitCodeDisassembler::GetVal2(bc);
                switch (op) {
                    case CC_EQ:
                        p_stack_->Set<mio_bool_t>(result, ::fabs(GetF64(val1) - GetF64(val2)) < DBL_EPSILON);
                        break;
                    case CC_NE:
                        p_stack_->Set<mio_bool_t>(result, ::fabs(GetF64(val1) - GetF64(val2)) >= DBL_EPSILON);
                        break;
                    case CC_LT:
                        p_stack_->Set<mio_bool_t>(result, GetF64(val1) < GetF64(val2));
                        break;
                    case CC_LE:
                        p_stack_->Set<mio_bool_t>(result, GetF64(val1) <= GetF64(val2));
                        break;
                    case CC_GT:
                        p_stack_->Set<mio_bool_t>(result, GetF64(val1) > GetF64(val2));
                        break;
                    case CC_GE:
                        p_stack_->Set<mio_bool_t>(result, GetF64(val1) >= GetF64(val2));
                        break;
                    default:
                        Panic(PANIC, ok, "bad comparator %d", op);
                        return;
                }
            } break;

            case BC_sext_i8: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
            #define DEFINE_CASE(byte, bit) \
                case byte: p_stack_->Set<mio_i##bit##_t>(result, GetI8(input));
                switch (bytes) {
                    MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
                    default:
                        Panic(BAD_BIT_CODE, ok, "i8 can not extend to i%d.", bytes * 8);
                        return;
                }
            #undef DEFINE_CASE
            } break;

            case BC_sext_i16: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
                switch (bytes) {
                    case 2:
                        p_stack_->Set(result, GetI16(input));
                        break;
                    case 4:
                        p_stack_->Set<mio_i32_t>(result, GetI16(input));
                        break;
                    case 8:
                        p_stack_->Set<mio_i64_t>(result, GetI16(input));
                        break;
                    default:
                        Panic(BAD_BIT_CODE, ok, "i16 can not extend to i%d.", bytes * 8);
                        return;
                }
            } break;

            case BC_sext_i32: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
                switch (bytes) {
                    case 4:
                        p_stack_->Set(result, GetI32(input));
                        break;
                    case 8:
                        p_stack_->Set<mio_i64_t>(result, GetI32(input));
                        break;
                    default:
                        Panic(BAD_BIT_CODE, ok, "i32 can not extend to i%d.", bytes * 8);
                        return;
                }
            } break;

            case BC_trunc_i16: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
                switch (bytes) {
                    case 2:
                        p_stack_->Set(result, GetI16(input));
                        break;
                    case 1:
                        p_stack_->Set(result, static_cast<mio_i8_t>(GetI16(input)));
                        break;
                    default:
                        Panic(BAD_BIT_CODE, ok, "i16 can not truncate to i%d.", bytes * 8);
                        return;
                }
            } break;

            case BC_trunc_i32: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
                switch (bytes) {
                    case 4:
                        p_stack_->Set(result, GetI32(input));
                        break;
                    case 2:
                        p_stack_->Set(result, static_cast<mio_i16_t>(GetI32(input)));
                        break;
                    case 1:
                        p_stack_->Set(result, static_cast<mio_i8_t>(GetI32(input)));
                        break;
                    default:
                        Panic(BAD_BIT_CODE, ok, "i32 can not truncate to i%d.", bytes * 8);
                        return;
                }
            } break;

            case BC_trunc_i64: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
            #define DEFINE_CASE(byte, bit)\
                case byte: \
                    p_stack_->Set(result, static_cast<mio_i##bit##_t>(GetI64(input)));
                switch (bytes) {
                    MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
                    default:
                        Panic(BAD_BIT_CODE, ok, "i64 can not truncate to i%d.", bytes * 8);
                        return;
                }
            #undef DEFINE_CASE
            } break;

            case BC_fpext_f32: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
                switch (bytes) {
                    case 4:
                        p_stack_->Set(result, GetF32(input));
                        break;
                    case 8:
                        p_stack_->Set(result, static_cast<mio_f64_t>(GetF32(input)));
                        break;
                    default:
                        Panic(BAD_BIT_CODE, ok, "f32 can not extend to f%d.", bytes * 8);
                        return;
                }
            } break;

            case BC_fptrunc_f64: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
                switch (bytes) {
                    case 8:
                        p_stack_->Set(result, GetF64(input));
                        break;
                    case 4:
                        p_stack_->Set(result, static_cast<mio_f32_t>(GetF64(input)));
                        break;
                    default:
                        Panic(BAD_BIT_CODE, ok, "f64 can not truncate to f%d.", bytes * 8);
                        return;
                }
            } break;

        #define DEFINE_CASE(byte, bit) \
            case BC_sitofp_i##bit: { \
                auto result = BitCodeDisassembler::GetOp1(bc); \
                auto bytes  = BitCodeDisassembler::GetOp2(bc); \
                auto input  = BitCodeDisassembler::GetImm32(bc); \
                switch (bytes) { \
                    case 4: \
                        p_stack_->Set(result, static_cast<mio_f32_t>(GetI##bit(input))); \
                        break; \
                    case 8: \
                        p_stack_->Set(result, static_cast<mio_f64_t>(GetI##bit(input))); \
                        break; \
                    default: \
                        Panic(BAD_BIT_CODE, ok, "i" #bit " can not cast to f%d.", bytes * 8); \
                        return; \
                } \
            } break;

            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

            case BC_fptosi_f32: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
                switch (bytes) {
                #define DEFINE_CASE(byte, bit) \
                    case byte: \
                        p_stack_->Set(result, static_cast<mio_i##bit##_t>(GetF32(input))); \
                        break;
                    MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
                #undef DEFINE_CASE
                    default:
                        Panic(BAD_BIT_CODE, ok, "f32 can not cast to i%d.", bytes * 8);
                        return;
                }
            } break;

            case BC_fptosi_f64: {
                auto result = BitCodeDisassembler::GetOp1(bc);
                auto bytes  = BitCodeDisassembler::GetOp2(bc);
                auto input  = BitCodeDisassembler::GetImm32(bc);
                switch (bytes) {
                #define DEFINE_CASE(byte, bit) \
                    case byte: \
                        p_stack_->Set(result, static_cast<mio_i##bit##_t>(GetF64(input))); \
                        break;
                    MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
                #undef DEFINE_CASE
                    default:
                        Panic(BAD_BIT_CODE, ok, "f64 can not cast to i%d.", bytes * 8);
                        return;
                }
            } break;

            case BC_frame: {
                auto size1 = BitCodeDisassembler::GetOp1(bc);
                auto size2 = BitCodeDisassembler::GetOp2(bc);

                p_stack_->AdjustFrame(0, size1);
                o_stack_->AdjustFrame(0, size2);

                auto clean2 = BitCodeDisassembler::GetVal2(bc);
                memset(o_stack_->offset(clean2), 0, size2 - clean2);
                vm_->gc_->Active(true);

                //RunGC();
            } break;

            case BC_ret: {
                auto ctx = call_stack_->Top();
                pc_     = ctx->pc;
                bc_     = ctx->bc;
                callee_ = ctx->callee;

                p_stack_->SetFrame(ctx->p_stack_base, ctx->p_stack_size);

                auto buf = o_stack_->frame<HeapObject *>();
                for (int i = 0; i < buf.n; ++i) {
                    buf.z[i] = nullptr;
                }
                o_stack_->SetFrame(ctx->o_stack_base, ctx->o_stack_size);
                call_stack_->Pop();
                if (call_stack_->size() == 0) {
                    exit_code_ = SUCCESS;
                    return;
                }
            } break;

            case BC_loop_entry: {
                auto id = BitCodeDisassembler::GetOp2(bc);
                auto native = BitCodeDisassembler::GetImm32(bc);
                if (vm_->jit_) {
                    if (native <= 0) {
                        int hit = 0;
                        TRACE(vm_->record_->TraceLoopEntry(generated_function(), id, pc_ - 1, &hit));
                        if (hit >= vm_->hot_loop_limit()) {
                            CompileToNativeCodeFragment(generated_function(), id, pc_ - 1, ok);
                            if (!*ok) {
                                return;
                            }
                        }
                    } else {


                    }
                }
            } break;

            case BC_jz: {
                auto id    = BitCodeDisassembler::GetOp1(bc);
                auto cond  = BitCodeDisassembler::GetOp2(bc);
                auto delta = BitCodeDisassembler::GetImm32(bc);

                auto value = p_stack_->Get<mio_bool_t>(cond);
                if (vm_->jit_ && id > 0) {
                    TRACE(vm_->record_->TraceGuardFalse(generated_function(), value, id, pc_ - 1));
                }

                if (value == 0) {
                    pc_ += delta - 1;
                }
            } break;

            case BC_jnz: {
                auto id    = BitCodeDisassembler::GetOp1(bc);
                auto cond  = BitCodeDisassembler::GetOp2(bc);
                auto delta = BitCodeDisassembler::GetImm32(bc);

                auto value = p_stack_->Get<mio_bool_t>(cond);
                if (vm_->jit_ && id > 0) {
                    TRACE(vm_->record_->TraceGuardTrue(generated_function(), value, id, pc_ - 1));
                }
                if (value != 0) {
                    pc_ += delta - 1;
                }
            } break;

            case BC_jmp: {
                auto linked_id = BitCodeDisassembler::GetOp1(bc);
                auto id = BitCodeDisassembler::GetOp2(bc);
                auto delta = BitCodeDisassembler::GetImm32(bc);
                if (vm_->jit_ && id > 0 && linked_id > 0) {
                    TRACE(vm_->record_->TraceLoopEdge(generated_function(), linked_id, id, pc_ - 1));
                }

                pc_ += delta - 1;
            } break;

            case BC_call_val: {
                if (call_stack_->size() >= vm_->max_call_deep()) {
                    Panic(STACK_OVERFLOW, ok, "stack overflow, max calling deep %d",
                          vm_->max_call_deep());
                    return;
                }

                auto obj_addr = BitCodeDisassembler::GetImm32(bc);
                Handle<HeapObject> ob(o_stack_->Get<HeapObject *>(obj_addr));
                DCHECK(ob->IsNativeFunction() || ob->IsGeneratedFunction() ||
                       ob->IsClosure()) << ob->GetKind();

                Handle<MIOFunction> fn;
                if (ob->IsClosure()) {
                    auto closure = ob->AsClosure();
                    fn = DCHECK_NOTNULL(closure->GetFunction());
                } else {
                    fn = static_cast<MIOFunction *>(ob.get());
                }

                if (fn->IsNativeFunction()) {
                    auto native = fn->AsNativeFunction();
                    if (!native->GetNativePointer()) {
                        Panic(NULL_NATIVE_FUNCTION, ok, "NULL native function!");
                        return;
                    }

                    auto ctx = call_stack_->Push();
                    ctx->p_stack_base = p_stack_->base_size();
                    ctx->p_stack_size = p_stack_->size();
                    ctx->o_stack_base = o_stack_->base_size();
                    ctx->o_stack_size = o_stack_->size();
                    ctx->pc           = pc_;
                    ctx->bc           = bc_;
                    ctx->callee       = callee_.get();

                    auto base1 = BitCodeDisassembler::GetOp1(bc);
                    auto base2 = BitCodeDisassembler::GetOp2(bc);
                    p_stack_->AdjustFrame(base1, native->GetPrimitiveArgumentsSize());
                    o_stack_->AdjustFrame(base2, native->GetObjectArgumentsSize());

                    callee_ = static_cast<MIOFunction*>(ob.get());
                    if (native->GetNativeWarperIndex()) {
                        (*native->GetNativeWarper())(this, native, p_stack_->offset(0), o_stack_->offset(0));
                    } else {
                        (*native->GetNativePointer())(vm_, this);
                    }
                    callee_ = ctx->callee;

                    p_stack_->SetFrame(ctx->p_stack_base, ctx->p_stack_size);
                    o_stack_->SetFrame(ctx->o_stack_base, ctx->o_stack_size);
                    call_stack_->Pop();
                    if (call_stack_->size() == 0) {
                        exit_code_ = SUCCESS;
                        return;
                    }
                } else {
                    auto ctx = call_stack_->Push();
                    ctx->p_stack_base = p_stack_->base_size();
                    ctx->p_stack_size = p_stack_->size();
                    ctx->o_stack_base = o_stack_->base_size();
                    ctx->o_stack_size = o_stack_->size();
                    ctx->pc = pc_;
                    ctx->bc = bc_;
                    ctx->callee = callee_.get();
                    callee_ = static_cast<MIOFunction*>(ob.get());

                    DCHECK(fn->IsGeneratedFunction()) << fn->GetKind();
                    auto normal = DCHECK_NOTNULL(fn->AsGeneratedFunction());
                    if (vm_->jit_) {
                        TRACE(vm_->record_->TraceFuncEntry(normal, 0));
                    }

                    auto base1 = BitCodeDisassembler::GetOp1(bc);
                    auto base2 = BitCodeDisassembler::GetOp2(bc);
                    p_stack_->AdjustFrame(base1, 0);
                    o_stack_->AdjustFrame(base2, 0);

                    pc_ = 0;
                    bc_ = static_cast<uint64_t *>(normal->GetCode());

                    vm_->gc_->Active(false);
                }
            } break;

            case BC_close_fn: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto closure = GetClosure(dest, ok);
                if (!*ok) {
                    Panic(PANIC, ok, "not closure for close.");
                    return;
                }

                if (closure->IsClose()) {
                    Panic(PANIC, ok, "closure already closed.");
                    return;
                }

                for (int i = 0; i < closure->GetUpValueSize(); i++) {
                    auto upval = closure->GetUpValue(i);

                    bool is_primitive = (upval->desc.unique_id & 0x1) == 0;
                    auto id = (upval->desc.unique_id >> 1) & 0x7fffffff;

                    const void *addr = nullptr;
                    if (is_primitive) {
                        addr = p_stack_->offset(upval->desc.offset);
                    } else {
                        addr = o_stack_->offset(upval->desc.offset);

                        vm_->gc_->WriteBarrier(closure.get(),
                                               o_stack_->Get<HeapObject *>(upval->desc.offset));
                    }
                    upval->val = vm_->gc_->GetOrNewUpValue(addr,
                            kMaxReferenceValueSize, id, is_primitive).get();
                    vm_->gc_->WriteBarrier(closure.get(), upval->val);
                }
                closure->Close(); // close closure;

                RunGC();
            } break;

            case BC_oop:
                ProcessObjectOperation(BitCodeDisassembler::GetOp1(bc),
                                       BitCodeDisassembler::GetOp2(bc),
                                       BitCodeDisassembler::GetVal1(bc),
                                       BitCodeDisassembler::GetVal2(bc), ok);
                if (!*ok) {
                    Panic(PANIC, ok, "oop process fail! %s",
                          kObjectOperatorText[BitCodeDisassembler::GetOp1(bc)]);
                    return;
                }
                break;

            default: {
                auto cmd = BitCodeDisassembler::GetInst(bc);
                if (cmd >= 0 && cmd < MAX_BC_INSTRUCTIONS) {
                    Panic(PANIC, ok, "bitcode command: \"%s\" not support yet.",
                          kInstructionMetadata[cmd].text);
                } else {
                    Panic(BAD_BIT_CODE, ok, "bad bit code command: %d", cmd);
                }
            } return;
        } // switch

        ++vm_->tick_;
    } // while
}

FunctionDebugInfo *Thread::GetDebugInfo(int layout, int *pc) {
    if (layout == 0) {
        *pc = pc_;
        return debug_info();
    }

    auto index = call_stack_->size() - layout;
    if (index < 0 || index >= call_stack_->size()) {
        DLOG(ERROR) << "layout out of range: " << layout;
        return 0;
    }

    auto ctx = call_stack_->base() + index;
    if (ctx->callee && ctx->debug_info()) {
        auto info = ctx->debug_info();
        *pc = ctx->pc - 1;
        DCHECK_GE(*pc, 0); DCHECK_LT(*pc, info->pc_size);
        return info;
    }
    return 0;
}

int Thread::GetCallStack(std::vector<MIOFunction *> *call_stack) {
    for (int i = 0; i < call_stack_->size(); ++i) {
        call_stack->push_back(call_stack_->base()[i].callee);
    }
    return call_stack_->size();
}

void Thread::Panic(ExitCode exit_code, bool *ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    PanicV(exit_code, ok, fmt, ap);
    va_end(ap);
}

void Thread::PanicV(ExitCode exit_code, bool *ok, const char *fmt, va_list ap) {
    exit_code_ = exit_code;
    *ok = false;

    vm_->backtrace_.clear(); {
        BacktraceLayout layout;
        layout.function_object = callee_.ToHandle();
        if (!callee_->IsNativeFunction()) {
            auto info = debug_info();
            if (info) {
                layout.file_name = vm_->object_factory()->GetOrNewString(info->file_name);

                auto pc = pc_ - 1;
                DCHECK_GE(pc, 0); DCHECK_LT(pc, info->pc_size);
                layout.position = info->pc_to_position[pc];
            }
        }
        vm_->backtrace_.push_back(layout);
    }

    int i = call_stack_->size();
    while (i--) {
        auto ctx = call_stack_->base() + i;
        if (!ctx->callee) {
            break;
        }

        BacktraceLayout layout;
        layout.function_object = ctx->callee;
        if (!ctx->callee->IsNativeFunction()) {
            auto info = ctx->debug_info();
            if (info) {
                layout.file_name = vm_->object_factory()->GetOrNewString(info->file_name);

                auto pc = ctx->pc - 1;
                DCHECK_GE(pc, 0); DCHECK_LT(pc, info->pc_size);
                layout.position = info->pc_to_position[pc];
            }
        }
        vm_->backtrace_.push_back(layout);
    }

    auto msg = TextOutputStream::vsprintf(fmt, ap);
    DLOG(ERROR) << "panic: (" << exit_code << ") " << msg;
}

void Thread::ProcessLoadPrimitive(int bytes, uint16_t dest, uint16_t segment,
                                  int32_t offset, bool *ok) {
    switch (static_cast<BCSegment>(segment)) {
        case BC_FUNCTION_CONSTANT_PRIMITIVE_SEGMENT: {
            auto buf = const_primitive_buf();
            if (offset < 0 || offset >= buf.n) {
                Panic(BAD_BIT_CODE, ok, "function: %s constant primitive data"
                      " out of range. %d vs. %d",
                      callee_->GetName()->GetData(), offset, buf.n);
                return;
            }
            FastMemoryMove(p_stack_->offset(dest), buf.z + offset, bytes);
        } break;

        case BC_UP_PRIMITIVE_SEGMENT: {
            if (!callee_->IsClosure()) {
                Panic(PANIC, ok, "callee is not closure, %d", callee_->GetKind());
                return;
            }
            auto buf = upvalue_buf();
            auto idx = offset / kObjectReferenceSize;
            if (idx < 0 || idx >= buf.n) {
                Panic(BAD_BIT_CODE, ok, "up value data out of range. %d vs %d",
                      idx, buf.n);
                return;
            }
            auto upval = buf.z[idx].val;
            if (upval->GetValueSize() < bytes) {
                Panic(BAD_BIT_CODE, ok, "upvalue size too small, %d vs. %d",
                      upval->GetValueSize(), bytes);
                return;
            }
            if (!upval->IsPrimitiveValue()) {
                Panic(BAD_BIT_CODE, ok, "upvalue is not primitive value!");
                return;
            }
            FastMemoryMove(p_stack_->offset(dest), upval->GetValue(), bytes);
        } break;

        case BC_GLOBAL_PRIMITIVE_SEGMENT:
            FastMemoryMove(p_stack_->offset(dest), vm_->p_global_->offset(offset), bytes);
            break;

        default:
            Panic(BAD_BIT_CODE, ok, "load_%db segment(%d) error. ", bytes,
                  segment);
            break;
    }
}

void Thread::ProcessStorePrimitive(int bytes, uint16_t addr, uint16_t segment,
                                   int dest, bool *ok) {
    auto src = p_stack_->offset(addr);
    switch (static_cast<BCSegment>(segment)) {
        case BC_GLOBAL_PRIMITIVE_SEGMENT:
            FastMemoryMove(vm_->p_global_->offset(dest), src, bytes);
            break;

        case BC_UP_PRIMITIVE_SEGMENT: {
            auto idx = dest / kObjectReferenceSize;
            auto buf = upvalue_buf();
            if (idx < 0 || idx >= buf.n) {
                Panic(BAD_BIT_CODE, ok, "up value data out of range. %d vs. %d",
                      idx, buf.n);
                return;
            }
            auto upval = buf.z[idx].val;
            if (upval->GetValueSize() < bytes) {
                Panic(BAD_BIT_CODE, ok, "upvalue size too small, %d vs. %d",
                      upval->GetValueSize(), bytes);
                return;
            }
            if (!upval->IsPrimitiveValue()) {
                Panic(BAD_BIT_CODE, ok, "upvalue is not primitive value!");
                return;
            }
            FastMemoryMove(upval->GetValue(), src, bytes);
        } break;

        default:
            Panic(BAD_BIT_CODE, ok, "store_%db segment(%d) error.", bytes, segment);
            break;
    }
}

void Thread::ProcessLoadObject(uint16_t dest, uint16_t segment, int32_t offset,
                               bool *ok) {
    switch (segment) {
        case BC_FUNCTION_CONSTANT_OBJECT_SEGMENT: {
            auto buf = const_object_buf();
            auto idx = offset / kObjectReferenceSize;
            if (idx < 0 || idx >= buf.n) {
                Panic(BAD_BIT_CODE, ok, "constant object data out of range. %d"
                      " vs. %d", idx, buf.n);
                return;
            }
            o_stack_->Set(dest, buf.z[idx]);
        } break;

        case BC_UP_OBJECT_SEGMENT: {
            auto buf = upvalue_buf();
            auto idx = offset / kObjectReferenceSize;
            if (idx < 0 || idx >= buf.n) {
                Panic(BAD_BIT_CODE, ok, "upvalue object data out of range. %d"
                      " vs. %d", idx, buf.n);
                return;
            }

            auto upval = buf.z[idx].val;
            if (!upval->IsObjectValue()) {
                Panic(BAD_BIT_CODE, ok, "upval is not object!");
                return;
            }

            o_stack_->Set(dest, upval->GetObject());
        } break;

        case BC_GLOBAL_OBJECT_SEGMENT:
            o_stack_->Set(dest, vm_->o_global_->Get<HeapObject *>(offset));
            break;

        default:
            DLOG(ERROR) << "load_o segment error.";
            break;
    }
}

void Thread::ProcessStoreObject(uint16_t addr, uint16_t segment, int dest, bool *ok) {
    auto src = make_handle(o_stack_->Get<HeapObject *>(addr));
    switch (static_cast<BCSegment>(segment)) {
        case BC_GLOBAL_OBJECT_SEGMENT:
            vm_->o_global_->Set(dest, src.get());
            break;

        case BC_UP_OBJECT_SEGMENT: {
            auto idx = dest / kObjectReferenceSize;
            auto buf = upvalue_buf();
            if (idx < 0 || idx >= buf.n) {
                Panic(BAD_BIT_CODE, ok, "up value data out of range. %d vs. %d",
                      idx, buf.n);
                return;
            }
            auto upval = buf.z[idx].val;
            if (upval->GetValueSize() < kObjectReferenceSize) {
                Panic(BAD_BIT_CODE, ok, "upvalue size too small, %d vs. %d",
                      upval->GetValueSize(), kObjectReferenceSize);
                return;
            }
            if (!upval->IsObjectValue()) {
                Panic(BAD_BIT_CODE, ok, "upvalue is not object value!");
                return;
            }
            upval->SetObject(src.get());
        } break;

        default:
            Panic(BAD_BIT_CODE, ok, "store_o segment(%d) error.", segment);
            break;
    }
}

void Thread::ProcessObjectOperation(int id, uint16_t result, int16_t val1,
                                    int16_t val2, bool *ok) {
    switch (static_cast<BCObjectOperatorId>(id)) {
        case OO_UnionOrMerge: {
            auto type_info = GetTypeInfo(val2, ok);
            if (!*ok) {
                return;
            }
            auto ob = CreateOrMergeUnion(val1, type_info, ok);
            if (!*ok) {
                return;
            }
            vm_->gc_->WriteBarrier(ob.get(), type_info.get());
            o_stack_->Set(result, ob.get());
            RunGC();
        } break;

        case OO_UnionTest: {
            auto type_info = GetTypeInfo(val2, ok);
            if (!*ok) {
                return;
            }
            auto ob = GetUnion(val1, ok);
            if (!*ok) {
                Panic(PANIC, ok, "object is not union, addr: %d", val1);
                return;
            }
            if (ob->GetTypeInfo() == type_info.get()) {
                p_stack_->Set<mio_bool_t>(result, 1);
            } else {
                p_stack_->Set<mio_bool_t>(result, 0);
            }
        } break;

        case OO_UnionUnbox: {
            auto type_info = GetTypeInfo(val2, ok);
            if (!*ok) {
                return;
            }
            auto ob = GetUnion(val1, ok);
            if (!*ok) {
                Panic(PANIC, ok, "object is not union, addr: %d", val1);
                return;
            }

            if (ob->GetTypeInfo() == type_info.get()) {
                if (type_info->IsPrimitive()) {
                    FastMemoryMove(p_stack_->offset(result), ob->GetData(),
                                   type_info->GetTypePlacementSize());
                } else {
                    FastMemoryMove(o_stack_->offset(result), ob->GetData(),
                                   kObjectReferenceSize);
                }
            } else {
                CreateEmptyValue(result, type_info, ok);
            }
            RunGC();
        } break;

        case OO_ToString: {
            auto type_info = GetTypeInfo(val2, ok);
            if (!*ok) {
                return;
            }
            std::string buf;
            MemoryOutputStream stream(&buf);
            NativeBaseLibrary::ToString(this, &stream,
                     type_info->IsPrimitive() ? p_stack_->offset(val1)
                     : o_stack_->offset(val1), type_info, ok);
            if (!*ok) {
                return;
            }

            auto ob = vm_->gc_->GetOrNewString(buf.data(),
                    static_cast<int>(buf.size()));
            if (ob.empty()) {
                Panic(OUT_OF_MEMORY, ok, "no memory for create string.");
                return;
            }
            o_stack_->Set(result, ob.get());

            RunGC();
        } break;

        case OO_StrCat: {
            auto lhs = GetString(val1, ok);
            if (lhs.empty()) {
                Panic(PANIC, ok, "object not string. addr: %d", val1);
                return;
            }

            auto rhs = GetString(val2, ok);
            if (rhs.empty()) {
                Panic(PANIC, ok, "object not string. addr: %d", val2);
                return;
            }

            mio_strbuf_t buf[2] = { lhs->Get(), rhs->Get() };
            auto rv = vm_->gc_->GetOrNewString(buf, arraysize(buf));
            if (rv.empty()) {
                Panic(OUT_OF_MEMORY, ok, "no memory for create string.");
                return;
            }
            o_stack_->Set(result, rv.get());

            RunGC();
        } break;

        case OO_StrLen: {
            auto ob = GetString(result, ok);
            if (ob.empty()) {
                Panic(PANIC, ok, "object not string. addr: %d", result);
                return;
            }
            p_stack_->Set<mio_int_t>(val2, ob->GetLength());
        } break;

        case OO_Array: {
            auto element = GetTypeInfo(val1, ok);
            if (!*ok) {
                return;
            }
            auto ob = vm_->object_factory()->CreateVector(val2, element);
            if (ob.empty()) {
                Panic(OUT_OF_MEMORY, ok, "no memory for create array.");
                return;
            }
            o_stack_->Set(result, ob.get());

            RunGC();
        } break;

        case OO_ArrayAdd: {
            auto ob = GetVector(result, ok);
            if (!*ok) {
                Panic(PANIC, ok, "incorrect object type, unexpected array.");
                return;
            }
            MIOArraySurface surface(ob, vm_->allocator());
            auto room = surface.AddRoom(1, ok);
            if (!*ok) {
                Panic(OUT_OF_MEMORY, ok, "no memory for add array element.");
                return;
            }

            if (ob->GetElement()->IsObject()) {
                FastMemoryMove(room, p_stack_->offset(val2),
                               ob->GetElement()->GetTypePlacementSize());
            } else {
                FastMemoryMove(room, o_stack_->offset(val2),
                               ob->GetElement()->GetTypePlacementSize());
            }

            RunGC();
        } break;

        case OO_ArraySet:
        case OO_ArrayDirectSet: {
            auto ob = GetObject(result);
            if (!ob->IsSlice() && !ob->IsVector()) {
                Panic(PANIC, ok, "incorrect object type, unexpected array or slice.");
                return;
            }

            mio_int_t index = -1;
            if (static_cast<BCObjectOperatorId>(id) == OO_ArrayDirectSet) {
                index = val1;
            } else {
                index = GetInt(val1);
            }
            MIOArraySurface surface(ob, vm_->allocator());
            if (index < 0 || index >= surface.size()) {
                Panic(PANIC, ok, "index out of range. %lld vs. [0, %d)", index,
                      surface.size());
                return;
            }

            void *src = nullptr;
            if (surface.element()->IsObject()) {
                src = o_stack_->offset(val2);
            } else {
                src = p_stack_->offset(val2);
            }
            FastMemoryMove(surface.RawGet(index), src, surface.element_size());
            if (surface.element()->IsObject()) {
                vm_->gc_->WriteBarrier(ob.get(), *static_cast<HeapObject **>(src));
            }
        } break;

        case OO_ArrayGet: {
            auto ob = GetObject(result);
            if (!ob->IsSlice() && !ob->IsVector()) {
                Panic(PANIC, ok, "incorrect object type, unexpected array or slice.");
                return;
            }

            MIOArraySurface surface(ob, vm_->allocator());
            auto index = p_stack_->Get<mio_int_t>(val1);
            if (index < 0 || index >= surface.size()) {
                Panic(PANIC, ok, "index out of range. %lld vs. [0, %d)", index,
                      surface.size());
                return;
            }

            void *dest = nullptr;
            if (surface.element()->IsObject()) {
                dest = o_stack_->offset(val2);
            } else {
                dest = p_stack_->offset(val2);
            }
            FastMemoryMove(dest, surface.RawGet(index), surface.element_size());
        } break;

        case OO_ArraySize: {
            auto ob = GetObject(result);
            if (!ob->IsSlice() && !ob->IsVector()) {
                Panic(PANIC, ok, "incorrect object type, unexpected array or slice.");
                return;
            }
            MIOArraySurface surface(ob, vm_->allocator());
            p_stack_->Set<mio_int_t>(val1, surface.size());
        } break;

        case OO_Slice: {
            auto ob = GetObject(result);
            if (!ob->IsSlice() && !ob->IsVector()) {
                Panic(PANIC, ok, "incorrect object type, unexpected array or slice.");
                return;
            }

            auto begin = static_cast<int>(p_stack_->Get<mio_int_t>(val1));
            auto size  = static_cast<int>(p_stack_->Get<mio_int_t>(val2));
            auto slice = vm_->object_factory()->CreateSlice(begin, size, ob);
            o_stack_->Set(result, slice.get());

            RunGC();
        } break;

        case OO_Map: {
            auto key = GetTypeInfo(val1, ok);
            auto value = GetTypeInfo(val2, ok);
            if (!*ok) {
                return;
            }
            auto ob = vm_->object_factory()->CreateHashMap(0, 7, key, value);
            if (ob.empty()) {
                Panic(OUT_OF_MEMORY, ok, "no memory for create map.");
                return;
            }
            o_stack_->Set(result, ob.get());

            RunGC();
        } break;

        case OO_MapWeak: {
            auto ob = GetHashMap(result, ok);
            if (!*ok) {
                Panic(PANIC, ok, "object is not map. addr: %d", result);
                return;
            }
            ob->SetWeakFlags(val1);
        } break;

        case OO_MapPut: {
            auto ob = GetHashMap(result, ok);
            if (!*ok) {
                Panic(PANIC, ok, "object is not map. addr: %d", result);
                return;
            }

            void *key = nullptr;
            if (ob->GetKey()->IsObject()) {
                key = o_stack_->offset(val1);
            } else {
                key = p_stack_->offset(val1);
            }

            void *value = nullptr;
            if (ob->GetValue()->IsObject()) {
                value = o_stack_->offset(val2);
            } else {
                value = p_stack_->offset(val2);
            }
            MIOHashMapSurface surface(ob.get(), vm_->allocator_);
            surface.RawPut(key, value, ok);
            if (!*ok) {
                Panic(OUT_OF_MEMORY, ok, "no memory for putting map key-value pair.");
                return;
            }
            if (ob->GetKey()->IsObject()) {
                vm_->gc_->WriteBarrier(ob.get(), *static_cast<HeapObject **>(key));
            }
            if (ob->GetValue()->IsObject()) {
                vm_->gc_->WriteBarrier(ob.get(), *static_cast<HeapObject **>(value));
            }

            RunGC();
        } break;

        case OO_MapDelete: {
            auto ob = GetHashMap(result, ok);
            if (!*ok) {
                Panic(PANIC, ok, "incorrect object type, unexpected map. addr: %d", result);
                return;
            }
            MIOHashMapSurface surface(ob.get(), vm_->allocator_);
            const void *key;
            if (ob->GetKey()->IsObject()) {
                key = o_stack_->offset(val1);
            } else {
                key = p_stack_->offset(val1);
            }
            p_stack_->Set<mio_bool_t>(val2, surface.RawDelete(key));
        } break;

        case OO_MapGet: {
            auto ob = GetHashMap(result, ok);
            if (!*ok) {
                Panic(PANIC, ok, "object not map. addr: %d", result);
                return;
            }
            MIOHashMapSurface surface(ob.get(), vm_->allocator_);
            const void *value = nullptr;
            if (ob->GetKey()->IsObject()) {
                value = surface.RawGet(o_stack_->offset(val1));
            } else {
                value = surface.RawGet(p_stack_->offset(val1));
            }
            Handle<MIOUnion> rv;
            if (value) {
                rv = vm_->object_factory()->CreateUnion(value,
                                                        ob->GetKey()->GetTypePlacementSize(),
                                                        make_handle(ob->GetValue()));
            } else {
                auto void_type = vm_->GetVoidType();
                if (!*ok) {
                    return;
                }
                rv = vm_->object_factory()->CreateUnion(nullptr, 0, void_type);
            }
            if (rv.empty()) {
                Panic(OUT_OF_MEMORY, ok, "no memory for create union.");
                return;
            }
            o_stack_->Set(val2, rv.get());

            RunGC();
        } break;

        case OO_MapFirstKey: {
            auto ob = GetHashMap(result, ok);
            if (!*ok) {
                Panic(PANIC, ok, "object not map. addr: %d", result);
                return;
            }

            MIOHashMapSurface surface(ob.get(), vm_->allocator_);
            auto pair = surface.GetNextRoom(nullptr);
            if (!pair) {
                return;
            }
            if (ob->GetKey()->IsObject()) {
                FastMemoryMove(o_stack_->offset(val1), pair->GetKey(),
                               ob->GetKey()->GetTypePlacementSize());
            } else {
                FastMemoryMove(p_stack_->offset(val1), pair->GetKey(),
                               ob->GetKey()->GetTypePlacementSize());
            }
            if (ob->GetValue()->IsObject()) {
                FastMemoryMove(o_stack_->offset(val2), pair->GetValue(),
                               ob->GetKey()->GetTypePlacementSize());
            } else {
                FastMemoryMove(p_stack_->offset(val2), pair->GetValue(),
                               ob->GetKey()->GetTypePlacementSize());
            }
            ++pc_;
        } break;

        case OO_MapNextKey: {
            auto ob = GetHashMap(result, ok);
            if (!*ok) {
                Panic(PANIC, ok, "object not map. addr: %d", result);
                return;
            }

            void *key = nullptr;
            if (ob->GetKey()->IsObject()) {
                key = o_stack_->offset(val1);
            } else {
                key = p_stack_->offset(val1);
            }
            MIOHashMapSurface surface(ob.get(), vm_->allocator_);
            auto pair = surface.GetNextRoom(key);
            if (!pair) {
                ++pc_;
                return;
            }
            FastMemoryMove(key, pair->GetKey(), ob->GetKey()->GetTypePlacementSize());

            if (ob->GetValue()->IsObject()) {
                FastMemoryMove(o_stack_->offset(val2), pair->GetValue(),
                               ob->GetKey()->GetTypePlacementSize());
            } else {
                FastMemoryMove(p_stack_->offset(val2), pair->GetValue(),
                               ob->GetKey()->GetTypePlacementSize());
            }
        } break;

        case OO_MapSize: {
            auto ob = GetHashMap(result, ok);
            if (!*ok) {
                Panic(PANIC, ok, "object not map. addr: %d", result);
                return;
            }
            MIOHashMapSurface surface(ob.get(), vm_->allocator_);
            p_stack_->Set<mio_int_t>(val2, surface.size());
        } break;

        default:
            *ok = false;
            break;
    }
}

Handle<MIOUnion>
Thread::CreateOrMergeUnion(int inbox, Handle<MIOReflectionType> reflection,
                           bool *ok) {
    switch (reflection->GetKind()) {
        case HeapObject::kReflectionVoid:
            return vm_->gc_->CreateUnion(nullptr, 0, reflection);

        case HeapObject::kReflectionUnion:
            return GetUnion(inbox, ok);

        case HeapObject::kReflectionIntegral:
            switch (reflection->AsReflectionIntegral()->GetBitWide()) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    return vm_->gc_->CreateUnion(p_stack_->offset(inbox), \
                            (byte), reflection);
                MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
            #undef DEFINE_CASE
                default:
                    Panic(BAD_BIT_CODE, ok, "bad integral bit size. %d",
                          reflection->AsReflectionIntegral()->GetBitWide());
                    goto fail;
            }
            break;

        case HeapObject::kReflectionFloating:
            switch (reflection->AsReflectionFloating()->GetBitWide()) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    return vm_->gc_->CreateUnion(p_stack_->offset(inbox), \
                            (byte), reflection);
                MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
            #undef DEFINE_CASE
                default:
                    Panic(BAD_BIT_CODE, ok, "bad floating bit size. %d",
                          reflection->AsReflectionFloating()->GetBitWide());
                    goto fail;
            }
            break;

        case HeapObject::kReflectionString:
        case HeapObject::kReflectionError:
        case HeapObject::kReflectionMap:
        case HeapObject::kReflectionFunction:
            return vm_->gc_->CreateUnion(o_stack_->offset(inbox),
                                         kObjectReferenceSize,
                                         reflection);


        default:
            Panic(BAD_BIT_CODE, ok, "bad type for reflection: %d",
                  reflection->GetKind());
            break;
    }
fail:
    return make_handle<MIOUnion>(nullptr);
}

void Thread::CreateEmptyValue(int result, Handle<MIOReflectionType> reflection,
                              bool *ok) {
    switch (reflection->GetKind()) {
        case HeapObject::kReflectionIntegral:
        case HeapObject::kReflectionFloating: {
            memset(p_stack_->offset(result), 0,
                   AlignDownBounds(kAlignmentSize, reflection->GetTypePlacementSize()));
        } break;

        case HeapObject::kReflectionString: {
            auto ob = vm_->gc_->GetOrNewString("", 0);
            o_stack_->Set(result, ob.get());
        } break;

        case HeapObject::kReflectionUnion: {
            auto ob = vm_->gc_->CreateUnion(nullptr, 0, vm_->GetVoidType());
            if (!*ok) {
                return;
            }
            o_stack_->Set(result, ob.get());
        } break;

        case HeapObject::kReflectionMap:
        case HeapObject::kReflectionError:
        case HeapObject::kReflectionFunction:
        case HeapObject::kReflectionVoid:
        default:
            Panic(PANIC, ok, "not support yet kind %d", reflection->GetKind());
            break;
    }
}

void Thread::CompileToNativeCodeFragment(MIOGeneratedFunction *fn, int id,
                                         int pc, bool *ok) {
    // TODO:
}

Handle<MIOReflectionType> Thread::GetTypeInfo(int index, bool *ok) {
    if (index < 0 || index >= vm_->all_type_->size()) {
        Panic(PANIC, ok, "type info index out of range.");
        return Handle<MIOReflectionType>();
    }
    return vm_->all_type_->Get(index);
}

} // namespace mio
