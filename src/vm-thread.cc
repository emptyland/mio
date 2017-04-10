#include "vm-thread.h"
#include "vm-object-factory.h"
#include "vm-objects.h"
#include "vm-stack.h"
#include "vm-memory-segment.h"
#include "vm-bitcode-disassembler.h"
#include "vm-bitcode.h"
#include "vm.h"
#include "memory-output-stream.h"
#include "handles.h"
#include "glog/logging.h"

namespace mio {

class CallStack {
public:
    static const int kSizeofElem = sizeof(CallContext);

    DEF_GETTER(int, size)

    CallContext *Push() {
        ++size_;
        return static_cast<CallContext *>(core_.AlignAdvance(kSizeofElem));
    }

    CallContext *Top() {
        DCHECK_GT(size_, 0);
        return core_.top<CallContext>() - 1;
    }

    void Pop() {
        --size_;
        core_.AdjustFrame(0, kSizeofElem);
    }

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

void Thread::Execute(MIONormalFunction *callee, bool *ok) {
    pc_ = 0;
    bc_ = static_cast<uint64_t *>(callee->GetCode());
    while (!should_exit_) {
        auto bc = bc_[pc_++];

        switch (BitCodeDisassembler::GetInst(bc)) {
            case BC_debug:
                *ok = false;
                exit_code_ = DEBUGGING;
                return;

            case BC_load_8b: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto segment = static_cast<BCSegment>(BitCodeDisassembler::GetOp2(bc));
                auto offset = BitCodeDisassembler::GetImm32(bc);

                if (segment == BC_CONSTANT_SEGMENT) {
                    memcpy(p_stack_->offset(dest), vm_->constants_->offset(offset), 8);
                } else if (segment == BC_GLOBAL_PRIMITIVE_SEGMENT) {
                    memcpy(p_stack_->offset(dest), vm_->p_global_->offset(offset), 8);
                } else {
                    DLOG(ERROR) << "load_8b segment error.";
                    exit_code_ = BAD_BIT_CODE;
                    *ok = false;
                }
            } break;

            case BC_load_o: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto offset = BitCodeDisassembler::GetImm32(bc);

                auto ob = vm_->o_global_->Get<HeapObject *>(offset);
                if (dest == 16) {
                    DLOG_IF(INFO, ob->IsString()) << ob->AsString()->GetData();
                }
                o_stack_->Set(dest, ob);
            } break;

            case BC_load_i32_imm: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto imm32 = BitCodeDisassembler::GetImm32(bc);

                p_stack_->Set(dest, imm32);
            } break;

            case BC_mov_8b: {
                auto dest = BitCodeDisassembler::GetVal1(bc);
                auto src  = BitCodeDisassembler::GetVal2(bc);

                memcpy(p_stack_->offset(dest), p_stack_->offset(src), 8);
            } break;

            case BC_mov_o: {
                auto dest = BitCodeDisassembler::GetVal1(bc);
                auto src  = BitCodeDisassembler::GetVal2(bc);

                auto ob = o_stack_->Get<HeapObject *>(src);
                o_stack_->Set(dest, ob);
            } break;

            case BC_add_i32_imm: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto lhs  = BitCodeDisassembler::GetOp2(bc);
                auto imm32 = BitCodeDisassembler::GetImm32(bc);

                p_stack_->Set(dest, GetI32(lhs) + imm32);
            } break;

            case BC_frame: {
                auto size1 = BitCodeDisassembler::GetOp1(bc);
                auto size2 = BitCodeDisassembler::GetOp2(bc);

                p_stack_->AdjustFrame(0, size1);
                o_stack_->AdjustFrame(0, size2);

                auto clean2 = BitCodeDisassembler::GetVal2(bc);
                if (clean2) {
                    memset(o_stack_->offset(clean2), 0, size2 - clean2);
                }
            } break;

            case BC_ret: {
                auto ctx = call_stack_->Top();
                pc_ = ctx->pc;
                bc_ = ctx->bc;

                p_stack_->SetFrame(ctx->p_stack_base, ctx->p_stack_size);
                o_stack_->SetFrame(ctx->o_stack_base, ctx->o_stack_size);
                call_stack_->Pop();
                if (call_stack_->size() == 0) {
                    exit_code_ = SUCCESS;
                    return;
                }
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
                ctx->pc = pc_;
                ctx->bc = bc_;

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
                Handle<HeapObject> obj(o_stack_->Get<HeapObject *>(obj_addr));
                DLOG_IF(INFO, obj->IsString()) << obj->AsString()->GetData();
                DCHECK(obj->IsNativeFunction() || obj->IsNormalFunction());

                if (obj->IsNativeFunction()) {
                    auto func = obj->AsNativeFunction();
                    if (!func->GetNativePointer()) {
                        *ok = false;
                        exit_code_ = NULL_NATIVE_FUNCTION;
                        return;
                    }

                    CallContext ctx = {
                        .p_stack_base = p_stack_->base_size(),
                        .p_stack_size = p_stack_->size(),
                        .o_stack_base = o_stack_->base_size(),
                        .o_stack_size = o_stack_->size(),
                    };

                    auto base1 = BitCodeDisassembler::GetOp1(bc);
                    auto base2 = BitCodeDisassembler::GetOp2(bc);
                    p_stack_->AdjustFrame(base1, func->GetPrimitiveArgumentsSize());
                    o_stack_->AdjustFrame(base2, func->GetObjectArgumentsSize());

                    (*func->GetNativePointer())(vm_, this);

                    p_stack_->SetFrame(ctx.p_stack_base, ctx.p_stack_size);
                    o_stack_->SetFrame(ctx.o_stack_base, ctx.o_stack_size);
                } else {
                    auto ctx = call_stack_->Push();
                    ctx->p_stack_base = p_stack_->base_size();
                    ctx->p_stack_size = p_stack_->size();
                    ctx->o_stack_base = o_stack_->base_size();
                    ctx->o_stack_size = o_stack_->size();
                    ctx->pc = pc_;
                    ctx->bc = bc_;

                    auto base1 = BitCodeDisassembler::GetOp1(bc);
                    auto base2 = BitCodeDisassembler::GetOp2(bc);
                    p_stack_->AdjustFrame(base1, 0);
                    o_stack_->AdjustFrame(base2, 0);

                    auto func = DCHECK_NOTNULL(obj->AsNormalFunction());
                    pc_ = 0;
                    bc_ = static_cast<uint64_t *>(func->GetCode());
                }
            } break;

            case BC_oop:
                ProcessObjectOperation(BitCodeDisassembler::GetOp1(bc),
                                       BitCodeDisassembler::GetOp2(bc),
                                       BitCodeDisassembler::GetVal1(bc),
                                       BitCodeDisassembler::GetVal2(bc), ok);
                if (!ok) {
                    DLOG(ERROR) << "oop process fail!";
                    exit_code_ = PANIC;
                }
                break;

            default: {
                auto cmd = BitCodeDisassembler::GetInst(bc);
                if (cmd >= 0 && cmd < MAX_BC_INSTRUCTIONS) {
                    DLOG(ERROR)
                        << "bitcode command: \""
                        << kInstructionMetadata[cmd].text
                        << "\" not support yet.";
                    exit_code_ = PANIC;
                } else {
                    DLOG(ERROR) << "bad bit code command: " << cmd;
                    exit_code_ = BAD_BIT_CODE;
                }
                *ok = false;
            } return;
        }
    }
}

mio_i8_t   Thread::GetI8(int addr)  { return p_stack_->Get<mio_i8_t>(addr); }
mio_i16_t  Thread::GetI16(int addr) { return p_stack_->Get<mio_i16_t>(addr); }
mio_i32_t  Thread::GetI32(int addr) { return p_stack_->Get<mio_i32_t>(addr); }
mio_i64_t  Thread::GetI64(int addr) { return p_stack_->Get<mio_i64_t>(addr); }
mio_f32_t  Thread::GetF32(int addr) { return p_stack_->Get<mio_f32_t>(addr); }
mio_f64_t  Thread::GetF64(int addr) { return p_stack_->Get<mio_f64_t>(addr); }

Handle<HeapObject> Thread::GetObject(int addr) {
    return make_handle(o_stack_->Get<HeapObject *>(addr));
}

Handle<MIOString> Thread::GetString(int addr, bool *ok) {
    auto obj = GetObject(addr);

    if (!obj->IsString()) {
        *ok = false;
        return make_handle<MIOString>(nullptr);
    }
    return make_handle(obj->AsString());
}

Handle<MIOError> Thread::GetError(int addr, bool *ok) {
    auto obj = GetObject(addr);

    if (!obj->IsError()) {
        *ok = false;
        return make_handle<MIOError>(nullptr);
    }
    return make_handle(obj->AsError());
}

Handle<MIOUnion> Thread::GetUnion(int addr, bool *ok) {
    auto obj = GetObject(addr);

    if (!obj->IsUnion()) {
        *ok = false;
        return make_handle<MIOUnion>(nullptr);
    }
    return make_handle(obj->AsUnion());
}

void Thread::ProcessObjectOperation(int id, uint16_t result, int16_t val1,
                                    int16_t val2, bool *ok) {
    switch (static_cast<BCObjectOperatorId>(id)) {
        case OO_UnionOrMerge: {
            auto type_info = GetTypeInfo(val2);
            auto obj = CreateOrMergeUnion(val1, type_info, ok);
        } break;

        case OO_ToString: {
            auto type_info = GetTypeInfo(val2);
            std::string buf;
            MemoryOutputStream stream(&buf);
            ToString(&stream,
                     type_info->IsPrimitive() ? p_stack_->offset(val1)
                     : o_stack_->offset(val1), type_info, ok);
            if (!*ok) {
                return;
            }

            auto rv = vm_->object_factory_->CreateString(
                    buf.data(), static_cast<int>(buf.size()));
            o_stack_->Set(result, rv.get());
        } break;

        case OO_StrCat: {
            auto lhs = GetString(val1, ok);
            if (lhs.empty()) {
                exit_code_ = PANIC;
                DLOG(ERROR) << "object not string. addr: " << val1;
                return;
            }

            auto rhs = GetString(val2, ok);
            if (rhs.empty()) {
                exit_code_ = PANIC;
                DLOG(ERROR) << "object not string. addr: " << val2;
                return;
            }

            mio_strbuf_t buf[2] = { lhs->Get(), rhs->Get() };
            auto rv = vm_->object_factory_->CreateString(buf, arraysize(buf));
            o_stack_->Set(result, rv.get());
        } break;

        default:
            *ok = false;
            break;
    }
}

int Thread::ToString(TextOutputStream *stream, void *addr,
                     Handle<MIOReflectionType> reflection, bool *ok) {
    switch (reflection->GetKind()) {
        case HeapObject::kReflectionIntegral: {
            switch (reflection->AsReflectionIntegral()->GetBitWide()) {
            #define DEFINE_CASE(byte, bit) \
                case bit: return stream->Printf("%" PRId##bit, *static_cast<mio_i##bit##_t *>(addr));
                MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
            #undef DEFINE_CASE
                default: goto fail;
            }
        } break;

        case HeapObject::kReflectionFloating:
            switch (reflection->AsReflectionFloating()->GetBitWide()) {
            #define DEFINE_CASE(byte, bit) \
                case bit: stream->Printf("%f", *static_cast<mio_f##bit##_t *>(addr));
                MIO_FLOAT_BYTES_TO_BITS(DEFINE_CASE)
            #undef DEFINE_CASE
                default: goto fail;
            }
            break;

        case HeapObject::kReflectionUnion: {
            auto ob = make_handle<MIOUnion>(*static_cast<MIOUnion **>(addr));
            return ToString(stream, ob->mutable_data(), make_handle(ob->GetTypeInfo()), ok);
        } break;

        case HeapObject::kReflectionString: {
            auto ob = make_handle<MIOString>(*static_cast<MIOString **>(addr));
            return stream->Write(ob->GetData(), ob->GetLength());
        } break;

        case HeapObject::kReflectionError:
        case HeapObject::kReflectionMap:
        case HeapObject::kReflectionFunction:

        fail:
        default:
            *ok = false;
            break;
    }
    return 0;
}

Handle<MIOUnion>
Thread::CreateOrMergeUnion(int inbox, Handle<MIOReflectionType> reflection,
                           bool *ok) {
    switch (reflection->GetKind()) {
        case HeapObject::kReflectionVoid:
            return vm_->object_factory_->CreateUnion(nullptr, 0, reflection);

        case HeapObject::kReflectionUnion:
            return GetUnion(inbox, ok);

        case HeapObject::kReflectionIntegral:
            switch (reflection->AsReflectionIntegral()->GetBitWide()) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    return vm_->object_factory_->CreateUnion(p_stack_->offset(inbox), \
                            (byte), reflection);
                MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
            #undef DEFINE_CASE
                default:
                    DLOG(ERROR) << "bad integral size.";
                    goto fail;
            }
            break;

        case HeapObject::kReflectionFloating:
            switch (reflection->AsReflectionFloating()->GetBitWide()) {
            #define DEFINE_CASE(byte, bit) \
                case bit: \
                    return vm_->object_factory_->CreateUnion(p_stack_->offset(inbox), \
                            (byte), reflection);
                MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
            #undef DEFINE_CASE
                default:
                    DLOG(ERROR) << "bad integral size.";
                    goto fail;
            }
            break;

        case HeapObject::kReflectionString:
        case HeapObject::kReflectionError:
        case HeapObject::kReflectionMap:
        case HeapObject::kReflectionFunction:
            return vm_->object_factory_->CreateUnion(o_stack_->offset(inbox),
                                                     kObjectReferenceSize,
                                                     reflection);

fail:
        default:
            *ok = false;
            break;
    }
    return make_handle<MIOUnion>(nullptr);
}

Handle<MIOReflectionType> Thread::GetTypeInfo(int index) {
    auto addr = vm_->type_info_base_ + index * sizeof(MIOReflectionType *);
    auto obj = make_handle(vm_->o_global_->Get<HeapObject *>(static_cast<int>(addr)));

    if (!obj->IsReflectionType()) {
        exit_code_ = PANIC;
        DLOG(ERROR) << "can not get reflection object! index = " << index;
        return make_handle<MIOReflectionType>(nullptr);
    }

    return make_handle(obj->AsReflectionType());
}

} // namespace mio
