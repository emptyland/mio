#include "vm-thread.h"
#include "vm-garbage-collector.h"
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

struct CallContext {
    int p_stack_base;
    int p_stack_size;
    int o_stack_base;
    int o_stack_size;

    MIOFunction *callee;

    int pc;
    uint64_t *bc;

    inline MIONormalFunction *normal_function() {
        auto fn = callee->AsNormalFunction();
        return fn ? fn : DCHECK_NOTNULL(callee->AsClosure())->GetFunction()->AsNormalFunction();
    }

    inline mio_buf_t<uint8_t> const_primitive_buf() {
        return DCHECK_NOTNULL(normal_function())->GetConstantPrimitiveBuf();
    }

    inline mio_buf_t<HeapObject *> const_object_buf() {
        return DCHECK_NOTNULL(normal_function())->GetConstantObjectBuf();
    }

    inline mio_buf_t<UpValDesc> upvalue_buf() {
        return DCHECK_NOTNULL(callee->AsClosure())->GetUpValuesBuf();
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
};

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

void Thread::Execute(MIONormalFunction *callee, bool *ok) {
    pc_ = 0;
    bc_ = static_cast<uint64_t *>(callee->GetCode());
    while (!should_exit_) {
        auto bc = bc_[pc_++];
        auto top = call_stack_->size() ? call_stack_->Top() : nullptr;

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
                ProcessLoadPrimitive(top, byte, dest, segment, offset, ok); \
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
                ProcessLoadObject(top, dest, segment, offset, ok);
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
            case BC_store_##byte##b: { \
                auto src = BitCodeDisassembler::GetOp1(bc); \
                auto segment = BitCodeDisassembler::GetOp2(bc); \
                auto dest = BitCodeDisassembler::GetImm32(bc); \
                ProcessStorePrimitive(top, byte, src, segment, dest, ok); \
                if (!*ok) { \
                    return; \
                } \
            } break;
            MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
        #undef DEFINE_CASE

            case BC_frame: {
                auto size1 = BitCodeDisassembler::GetOp1(bc);
                auto size2 = BitCodeDisassembler::GetOp2(bc);

                p_stack_->AdjustFrame(0, size1);
                o_stack_->AdjustFrame(0, size2);

                auto clean2 = BitCodeDisassembler::GetVal2(bc);
                memset(o_stack_->offset(clean2), 0, size2 - clean2);
                vm_->gc_->Active(false);
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

            case BC_jz: {
                auto cond = BitCodeDisassembler::GetOp2(bc);
                auto delta = BitCodeDisassembler::GetImm32(bc);

                if (p_stack_->Get<mio_i8_t>(cond) == 0) {
                    pc_ += delta - 1;
                }
            } break;

            case BC_jnz: {
                auto cond = BitCodeDisassembler::GetOp2(bc);
                auto delta = BitCodeDisassembler::GetImm32(bc);

                if (p_stack_->Get<mio_i8_t>(cond) != 0) {
                    pc_ += delta - 1;
                }
            } break;

            case BC_jmp: {
                auto delta = BitCodeDisassembler::GetImm32(bc);
                pc_ += delta - 1;
            } break;

            case BC_call_val: {
                if (call_stack_->size() >= vm_->max_call_deep()) {
                    *ok = false;
                    exit_code_ = STACK_OVERFLOW;
                    return;
                }

                auto obj_addr = BitCodeDisassembler::GetImm32(bc);
                Handle<HeapObject> ob(o_stack_->Get<HeapObject *>(obj_addr));
                DLOG_IF(INFO, ob->IsString()) << ob->AsString()->GetData();
                DCHECK(ob->IsNativeFunction() || ob->IsNormalFunction() ||
                       ob->IsClosure());

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
                        *ok = false;
                        exit_code_ = NULL_NATIVE_FUNCTION;
                        return;
                    }

                    CallContext ctx = {
                        .p_stack_base = p_stack_->base_size(),
                        .p_stack_size = p_stack_->size(),
                        .o_stack_base = o_stack_->base_size(),
                        .o_stack_size = o_stack_->size(),
                        .callee       = static_cast<MIOFunction*>(ob.get()),
                    };

                    auto base1 = BitCodeDisassembler::GetOp1(bc);
                    auto base2 = BitCodeDisassembler::GetOp2(bc);
                    p_stack_->AdjustFrame(base1, native->GetPrimitiveArgumentsSize());
                    o_stack_->AdjustFrame(base2, native->GetObjectArgumentsSize());

                    (*native->GetNativePointer())(vm_, this);

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
                    ctx->callee = static_cast<MIOFunction*>(ob.get());

                    auto normal = DCHECK_NOTNULL(fn->AsNormalFunction());
                    auto base1 = BitCodeDisassembler::GetOp1(bc);
                    auto base2 = BitCodeDisassembler::GetOp2(bc);
                    p_stack_->AdjustFrame(base1, 0);
                    o_stack_->AdjustFrame(base2, 0);

                    pc_ = 0;
                    bc_ = static_cast<uint64_t *>(normal->GetCode());

                    vm_->gc_->Active(true);
                }
            } break;

            case BC_close_fn: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto closure = GetClosure(dest, ok);
                if (!*ok) {
                    DLOG(ERROR) << "not closure for close.";
                    exit_code_ = PANIC;
                    return;
                }

                if (closure->IsClose()) {
                    DLOG(ERROR) << "closure already closed.";
                    exit_code_ = PANIC;
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
            } break;

            case BC_oop:
                ProcessObjectOperation(BitCodeDisassembler::GetOp1(bc),
                                       BitCodeDisassembler::GetOp2(bc),
                                       BitCodeDisassembler::GetVal1(bc),
                                       BitCodeDisassembler::GetVal2(bc), ok);
                if (!*ok) {
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
        } // switch

        vm_->gc_->Step(++vm_->tick_);
    } // while
}

int Thread::GetCallStack(std::vector<MIOFunction *> *call_stack) {
    for (int i = 0; i < call_stack_->size(); ++i) {
        call_stack->push_back(call_stack_->base()[i].callee);
    }
    return call_stack_->size();
}

void Thread::ProcessLoadPrimitive(CallContext *top, int bytes, uint16_t dest,
                                  uint16_t segment, int32_t offset, bool *ok) {
    switch (static_cast<BCSegment>(segment)) {
        case BC_FUNCTION_CONSTANT_PRIMITIVE_SEGMENT: {
            auto buf = top->const_primitive_buf();
            if (offset < 0 || offset >= buf.n) {
                DLOG(ERROR) << "function: " << top->callee->GetName()->GetData()
                            << " constant primitive data out of range. "
                            << offset << " vs. " << buf.n;
                goto fail;
            }
            memcpy(p_stack_->offset(dest), buf.z + offset, bytes);
        } return;

        case BC_UP_PRIMITIVE_SEGMENT: {
            auto buf = top->upvalue_buf();
            auto idx = offset / kObjectReferenceSize;
            if (idx < 0 || idx >= buf.n) {
                DLOG(ERROR) << "up value data out of range. "
                            << idx << " vs. " << buf.n;
                goto fail;
            }
            auto upval = buf.z[idx].val;
            if (upval->GetValueSize() < bytes) {
                DLOG(ERROR) << "upvalue size too small, " << upval->GetValueSize()
                            << " vs. " << bytes;
                goto fail;
            }
            if (!upval->IsPrimitiveValue()) {
                DLOG(ERROR) << "upvalue is not primitive value!";
                goto fail;
            }
            memcpy(p_stack_->offset(dest), upval->GetValue(), bytes);
        } return;

        case BC_GLOBAL_PRIMITIVE_SEGMENT:
            memcpy(p_stack_->offset(dest), vm_->p_global_->offset(offset), bytes);
            return;

        default:
            DLOG(ERROR) << "load_xb segment error.";
            break;
    }
fail:
    exit_code_ = BAD_BIT_CODE;
    *ok = false;
}

void Thread::ProcessStorePrimitive(CallContext *top, int bytes, uint16_t addr,
                                   uint16_t segment, int dest, bool *ok) {
    auto src = p_stack_->offset(addr);
    switch (static_cast<BCSegment>(segment)) {
        case BC_GLOBAL_PRIMITIVE_SEGMENT:
            memcpy(vm_->p_global_->offset(dest), src, bytes);
            return;

        case BC_UP_PRIMITIVE_SEGMENT: {
            auto idx = dest / kObjectReferenceSize;
            auto buf = top->upvalue_buf();
            if (idx < 0 || idx >= buf.n) {
                DLOG(ERROR) << "up value data out of range. "
                << idx << " vs. " << buf.n;
                goto fail;
            }
            auto upval = buf.z[idx].val;
            if (upval->GetValueSize() < bytes) {
                DLOG(ERROR) << "upvalue size too small, " << upval->GetValueSize()
                << " vs. " << bytes;
                goto fail;
            }
            if (!upval->IsPrimitiveValue()) {
                DLOG(ERROR) << "upvalue is not primitive value!";
                goto fail;
            }
            memcpy(upval->GetValue(), src, bytes);
        } return;

        default:
            DLOG(ERROR) << "store_xb segment error." << segment;
            break;
    }
fail:
    exit_code_ = BAD_BIT_CODE;
    *ok = false;
}

void Thread::ProcessLoadObject(CallContext *top, uint16_t dest, uint16_t segment,
                               int32_t offset, bool *ok) {
    switch (segment) {
        case BC_FUNCTION_CONSTANT_OBJECT_SEGMENT: {
            auto buf = top->const_object_buf();
            auto idx = offset / kObjectReferenceSize;
            if (idx < 0 || idx >= buf.n) {
                DLOG(ERROR) << "constant object data out of range. " << idx
                            << " vs. " <<  buf.n;
                goto fail;
            }
            o_stack_->Set(dest, buf.z[idx]);
        } return;

        case BC_UP_OBJECT_SEGMENT: {
            auto buf = top->upvalue_buf();
            auto idx = offset / kObjectReferenceSize;
            if (idx < 0 || idx >= buf.n) {
                DLOG(ERROR) << "upvalue object data out of range. " << idx
                            << " vs. " << buf.n;
                goto fail;
            }

            auto upval = buf.z[idx].val;
            if (!upval->IsObjectValue()) {
                DLOG(ERROR) << "upval is not object!";
                goto fail;
            }

            o_stack_->Set(dest, upval->GetObject());
        } return;

        case BC_GLOBAL_OBJECT_SEGMENT:
            o_stack_->Set(dest, vm_->o_global_->Get<HeapObject *>(offset));
            return;

        default:
            DLOG(ERROR) << "load_o segment error.";
            break;
    }
fail:
    exit_code_ = BAD_BIT_CODE;
    *ok = false;
}

void Thread::ProcessStoreObject(CallContext *top, uint16_t addr,
                                uint16_t segment, int dest, bool *ok) {
    auto src = make_handle(o_stack_->Get<HeapObject *>(addr));
    switch (static_cast<BCSegment>(segment)) {
        case BC_GLOBAL_PRIMITIVE_SEGMENT:
            vm_->o_global_->Set(dest, src.get());
            return;

        case BC_UP_PRIMITIVE_SEGMENT: {
            auto idx = dest / kObjectReferenceSize;
            auto buf = top->upvalue_buf();
            if (idx < 0 || idx >= buf.n) {
                DLOG(ERROR) << "up value data out of range. "
                << idx << " vs. " << buf.n;
                goto fail;
            }
            auto upval = buf.z[idx].val;
            if (upval->GetValueSize() < kObjectReferenceSize) {
                DLOG(ERROR) << "upvalue size too small, " << upval->GetValueSize()
                            << " vs. " << kObjectReferenceSize;
                goto fail;
            }
            if (!upval->IsObjectValue()) {
                DLOG(ERROR) << "upvalue is not object value!";
                goto fail;
            }
            upval->SetObject(src.get());
        } return;

        default:
            DLOG(ERROR) << "store_xb segment error." << segment;
            break;
    }
fail:
    exit_code_ = BAD_BIT_CODE;
    *ok = false;
}

void Thread::ProcessObjectOperation(int id, uint16_t result, int16_t val1,
                                    int16_t val2, bool *ok) {
    switch (static_cast<BCObjectOperatorId>(id)) {
        case OO_UnionOrMerge: {
            auto type_info = GetTypeInfo(val2);
            auto ob = CreateOrMergeUnion(val1, type_info, ok);
            vm_->gc_->WriteBarrier(ob.get(), type_info.get());
            o_stack_->Set(result, ob.get());
        } break;

        case OO_UnionTest: {
            auto type_info = GetTypeInfo(val2);
            auto ob = GetUnion(val1, ok);
            if (!*ok) {
                return;
            }
            if (ob->GetTypeInfo() == type_info.get()) {
                p_stack_->Set<mio_i8_t>(result, 1);
            } else {
                p_stack_->Set<mio_i8_t>(result, 0);
            }
        } break;

        case OO_UnionUnbox: {
            auto type_info = GetTypeInfo(val2);
            auto ob = GetUnion(val1, ok);
            if (!*ok) {
                return;
            }

            if (ob->GetTypeInfo() == type_info.get()) {
                if (type_info->IsPrimitive()) {
                    memcpy(p_stack_->offset(result), ob->GetData(),
                           type_info->GetPlacementSize());
                } else {
                    memcpy(o_stack_->offset(result), ob->GetData(),
                           kObjectReferenceSize);
                }
            } else {
                CreateEmptyValue(result, type_info, ok);
            }
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

            auto ob = vm_->gc_->GetOrNewString(buf.data(),
                    static_cast<int>(buf.size()));
            o_stack_->Set(result, ob.get());
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
            auto rv = vm_->gc_->CreateString(buf, arraysize(buf));
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
                default:
                    DLOG(ERROR) << "bad integral bitwide: " <<
                        reflection->AsReflectionIntegral()->GetBitWide();
                    goto fail;
            }
        } break;

        case HeapObject::kReflectionFloating:
            switch (reflection->AsReflectionFloating()->GetBitWide()) {
            #define DEFINE_CASE(byte, bit) \
                case bit: return stream->Printf("%0.5f", *static_cast<mio_f##bit##_t *>(addr));
                MIO_FLOAT_BYTES_TO_BITS(DEFINE_CASE)
            #undef DEFINE_CASE
                default:
                    DLOG(ERROR) << "bad floating bitwide: " <<
                            reflection->AsReflectionFloating()->GetBitWide();
                    goto fail;
            }
            break;

        case HeapObject::kReflectionUnion: {
            auto ob = make_handle<MIOUnion>(*static_cast<MIOUnion **>(addr));
            return ToString(stream, ob->GetMutableData(), make_handle(ob->GetTypeInfo()), ok);
        } break;

        case HeapObject::kReflectionString: {
            auto ob = make_handle<MIOString>(*static_cast<MIOString **>(addr));
            return stream->Write(ob->GetData(), ob->GetLength());
        } break;

        case HeapObject::kReflectionVoid:
            return stream->Write("[void]", 6);

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
                    DLOG(ERROR) << "bad integral size.";
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
                    DLOG(ERROR) << "bad integral size.";
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

fail:
        default:
            *ok = false;
            break;
    }
    return make_handle<MIOUnion>(nullptr);
}

void Thread::CreateEmptyValue(int result, Handle<MIOReflectionType> reflection,
                              bool *ok) {
    switch (reflection->GetKind()) {
        case HeapObject::kReflectionIntegral:
        case HeapObject::kReflectionFloating: {
            memset(p_stack_->offset(result), 0,
                   AlignDownBounds(kAlignmentSize, reflection->GetPlacementSize()));
        } break;

        case HeapObject::kReflectionString: {
            auto ob = vm_->gc_->GetOrNewString("", 0);
            o_stack_->Set(result, ob.get());
        } break;

        case HeapObject::kReflectionUnion: {
            auto ob = vm_->gc_->CreateUnion(
                    nullptr, 0, GetTypeInfo(vm_->type_void_index));
            o_stack_->Set(result, ob.get());
        } break;

        case HeapObject::kReflectionMap:
        case HeapObject::kReflectionError:
        case HeapObject::kReflectionFunction:
        case HeapObject::kReflectionVoid:
            DLOG(ERROR) << "not support yet.";
        default:
            *ok = false;
            break;
    }
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
