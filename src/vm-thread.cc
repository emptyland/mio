#include "vm-thread.h"
#include "vm-objects.h"
#include "vm-stack.h"
#include "vm-memory-segment.h"
#include "vm-bitcode-disassembler.h"
#include "vm-bitcode.h"
#include "vm.h"
#include "handles.h"
#include "glog/logging.h"

namespace mio {

class CallStack {
public:
    static const int kSizeofElem = sizeof(CallContext);

    DEF_GETTER(int, size)

    CallContext *Push() {
        return static_cast<CallContext *>(core_.AlignAdvance(kSizeofElem));
    }

    CallContext *Top() {
        DCHECK_GT(size_, 0);
        return core_.top<CallContext>() - 1;
    }

    void Pop() { core_.AdjustFrame(0, kSizeofElem); }

private:
    int size_ = 0;
    Stack core_;
};

Thread::Thread(VM *vm)
    : vm_(DCHECK_NOTNULL(vm))
    , p_stack_(new Stack())
    , o_stack_(new Stack())
    , call_stack_(new CallStack()) {
}

Thread::~Thread() {
    delete p_stack_;
    delete o_stack_;
}

void Thread::Execute(int pc, bool *ok) {
    DCHECK_LT(pc * 8, vm_->code_->size());
    pc_ = pc;

    uint64_t *pbc = static_cast<uint64_t *>(vm_->code_->offset(pc_));
    while (!should_exit_) {
        auto bc = pbc[pc_++];

        switch (BitCodeDisassembler::GetInst(bc)) {
            case BC_debug:
                *ok = false;
                exit_code_ = DEBUGGING;
                return;

            case BC_load_i32_imm: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto imm32 = BitCodeDisassembler::GetImm32(bc);

                p_stack_->Set(dest, imm32);
            } break;

            case BC_add_i32_imm: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto lhs  = BitCodeDisassembler::GetOp2(bc);
                auto imm32 = BitCodeDisassembler::GetImm32(bc);

                p_stack_->Set(dest, lhs + imm32);
            } break;

            case BC_frame: {
                auto size1 = BitCodeDisassembler::GetVal1(bc);
                auto size2 = BitCodeDisassembler::GetVal2(bc);

                p_stack_->AdjustFrame(0, size1);
                o_stack_->AdjustFrame(0, size2);
            } break;

            case BC_ret: {
                if (call_stack_->size() == 0) {
                    exit_code_ = SUCCESS;
                    return;
                }
                auto ctx = call_stack_->Top();
                pc_ = ctx->pc;

                p_stack_->SetFrame(ctx->p_stack_base, ctx->p_stack_size);
                o_stack_->SetFrame(ctx->o_stack_base, ctx->o_stack_size);
                call_stack_->Pop();
            } break;

                // [-5] p_stack_base
                // [-4] p_stack_size
                // [-3] o_stack_base
                // [-2] o_stack_size
                // [-1] pc
                // [0]  arg1 <- new frame
                // [1]  arg2
                // [3]  arg3
            case BC_call: {
                if (call_stack_->size() >= vm_->max_call_deep()) {
                    *ok = false;
                    exit_code_ = STACK_OVERFLOW;
                    return;
                }
                auto ctx = call_stack_->Push();
                ctx->p_stack_base = p_stack_->base_size();
                ctx->p_stack_size = p_stack_->size();
                ctx->o_stack_base = o_stack_->base_size();
                ctx->o_stack_size = o_stack_->size();
                ctx->pc = pc_ + 1;

                auto base1 = BitCodeDisassembler::GetOp1(bc);
                auto base2 = BitCodeDisassembler::GetOp2(bc);
                p_stack_->AdjustFrame(base1, 0);
                o_stack_->AdjustFrame(base2, 0);

                auto delta = BitCodeDisassembler::GetImm32(bc);
                pc_ += delta;
            } break;

            case BC_call_val: {
                if (call_stack_->size() >= vm_->max_call_deep()) {
                    *ok = false;
                    exit_code_ = STACK_OVERFLOW;
                    return;
                }

                auto obj_addr = BitCodeDisassembler::GetImm32(bc);
                Local<HeapObject> obj(static_cast<HeapObject *>(o_stack_->offset(obj_addr)));
                DCHECK(obj->IsNativeFunction() || obj->IsNormalFunction());

                if (obj->IsNativeFunction()) {
                    auto func = obj->AsNativeFunction();
                    if (!func->GetNativePointer()) {
                        *ok = false;
                        exit_code_ = NULL_NATIVE_FUNCTION;
                        return;
                    }
                    (*func->GetNativePointer())(vm_, this);
                } else {
                    auto ctx = call_stack_->Push();
                    ctx->p_stack_base = p_stack_->base_size();
                    ctx->p_stack_size = p_stack_->size();
                    ctx->o_stack_base = o_stack_->base_size();
                    ctx->o_stack_size = o_stack_->size();
                    ctx->pc = pc_ + 1;

                    auto base1 = BitCodeDisassembler::GetOp1(bc);
                    auto base2 = BitCodeDisassembler::GetOp2(bc);
                    p_stack_->AdjustFrame(base1, 0);
                    o_stack_->AdjustFrame(base2, 0);

                    auto func = obj->AsNormalFunction();
                    pc_ += func->GetAddress();
                }
            } break;

            default:
                *ok = false;
                exit_code_ = PANIC;
                return;
        }
    }
}

} // namespace mio
