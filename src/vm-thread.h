#ifndef MIO_VM_THREAD_H_
#define MIO_VM_THREAD_H_

#include "vm-objects.h"
#include "vm-stack.h"
#include "handles.h"
#include "base.h"
#include <stdarg.h>

namespace mio {

class VM;
struct CallContext;
class CallStack;

class TextOutputStream;

class Thread {
public:
    enum ExitCode: int {
        SUCCESS,
        DEBUGGING,
        PANIC,
        STACK_OVERFLOW,
        NULL_NATIVE_FUNCTION,
        BAD_BIT_CODE,
        OUT_OF_MEMORY,
        DIV_ZERO,
    };

    Thread(VM *vm);
    ~Thread();

    DEF_GETTER(ExitCode, exit_code)
    DEF_PROP_RW(bool, should_exit)
    DEF_PROP_RW(int, syscall)

    Stack *p_stack() const { return p_stack_; }
    Stack *o_stack() const { return o_stack_; }

    VM *vm() const { return vm_; }

    MIOFunction *callee() const { return callee_.get(); }

    void Execute(MIOGeneratedFunction *callee, bool *ok);

    inline mio_bool_t GetBool(int addr) { return GetI8(addr); }
    inline mio_i8_t   GetI8(int addr);
    inline mio_i16_t  GetI16(int addr);
    inline mio_i32_t  GetI32(int addr);
    inline mio_int_t  GetInt(int addr) { return GetI64(addr); }
    inline mio_i64_t  GetI64(int addr);

    inline mio_f32_t  GetF32(int addr);
    inline mio_f64_t  GetF64(int addr);

    inline Handle<HeapObject> GetObject(int addr);
    inline Handle<MIOString>  GetString(int addr, bool *ok);
    inline Handle<MIOError>   GetError(int addr, bool *ok);
    inline Handle<MIOUnion>   GetUnion(int addr, bool *ok);
    inline Handle<MIOClosure> GetClosure(int addr, bool *ok);
    inline Handle<MIOVector>  GetVector(int addr, bool *ok);
    inline Handle<MIOHashMap> GetHashMap(int addr, bool *ok);

    inline int GetSourcePosition(int layout);
    inline const char *GetSourceFileName(int layout);

    int GetCallStack(std::vector<MIOFunction *> *call_stack);

    __attribute__ (( __format__ (__printf__, 4, 5)))
    void Panic(ExitCode exit_code, bool *ok, const char *fmt, ...);

    void PanicV(ExitCode exit_code, bool *ok, const char *fmt, va_list ap);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Thread)
private:
    Handle<MIOReflectionType> GetTypeInfo(int index, bool *ok);

    FunctionDebugInfo *GetDebugInfo(int layout, int *pc);

    void ProcessLoadPrimitive(int bytes, uint16_t dest, uint16_t segment,
                              int32_t offset, bool *ok);
    void ProcessStorePrimitive(int bytes, uint16_t addr, uint16_t segment,
                               int dest, bool *ok);
    void ProcessLoadObject(uint16_t dest, uint16_t segment, int32_t offset,
                           bool *ok);
    void ProcessStoreObject(uint16_t addr, uint16_t segment, int dest, bool *ok);
    void ProcessObjectOperation(int id, uint16_t result, int16_t val1,
                                int16_t val2, bool *ok);

    Handle<MIOUnion> CreateOrMergeUnion(int inbox,
                                        Handle<MIOReflectionType> reflection,
                                        bool *ok);
    void CreateEmptyValue(int result,
                          Handle<MIOReflectionType> reflection, bool *ok);

    inline MIOGeneratedFunction *generated_function();
    inline mio_buf_t<uint8_t> const_primitive_buf();
    inline mio_buf_t<HeapObject *> const_object_buf();
    inline mio_buf_t<UpValDesc> upvalue_buf();
    inline FunctionDebugInfo *debug_info();

    VM *vm_;
    int syscall_ = 0;
    Stack *p_stack_;
    Stack *o_stack_;
    CallStack *call_stack_;
    int pc_ = 0;
    uint64_t *bc_ = nullptr;
    AtomicHandle<MIOFunction> callee_;
    bool should_exit_ = false;
    ExitCode exit_code_ = SUCCESS;
}; // class Thread


////////////////////////////////////////////////////////////////////////////////
/// Inline Functions:
////////////////////////////////////////////////////////////////////////////////
inline int Thread::GetSourcePosition(int layout) {
    int pc = 0;
    auto info = GetDebugInfo(layout, &pc);
    if (info) {
        return info->pc_to_position[pc];
    } else {
        return 0;
    }
}

inline const char *Thread::GetSourceFileName(int layout) {
    int pc = 0;
    auto info = GetDebugInfo(layout, &pc);
    if (info) {
        return info->file_name;
    } else {
        return "";
    }
}

inline mio_i8_t   Thread::GetI8(int addr)  { return p_stack_->Get<mio_i8_t>(addr); }
inline mio_i16_t  Thread::GetI16(int addr) { return p_stack_->Get<mio_i16_t>(addr); }
inline mio_i32_t  Thread::GetI32(int addr) { return p_stack_->Get<mio_i32_t>(addr); }
inline mio_i64_t  Thread::GetI64(int addr) { return p_stack_->Get<mio_i64_t>(addr); }
inline mio_f32_t  Thread::GetF32(int addr) { return p_stack_->Get<mio_f32_t>(addr); }
inline mio_f64_t  Thread::GetF64(int addr) { return p_stack_->Get<mio_f64_t>(addr); }

inline Handle<HeapObject> Thread::GetObject(int addr) {
    return make_handle(o_stack_->Get<HeapObject *>(addr));
}

inline Handle<MIOString> Thread::GetString(int addr, bool *ok) {
    auto ob = GetObject(addr);

    if (!ob->IsString()) {
        *ok = false;
        return make_handle<MIOString>(nullptr);
    }
    return make_handle(ob->AsString());
}

inline Handle<MIOError> Thread::GetError(int addr, bool *ok) {
    auto ob = GetObject(addr);

    if (!ob->IsError()) {
        *ok = false;
        return make_handle<MIOError>(nullptr);
    }
    return make_handle(ob->AsError());
}

inline Handle<MIOUnion> Thread::GetUnion(int addr, bool *ok) {
    auto ob = GetObject(addr);

    if (ob.empty() || !ob->IsUnion()) {
        *ok = false;
        return make_handle<MIOUnion>(nullptr);
    }
    return make_handle(ob->AsUnion());
}

inline Handle<MIOClosure> Thread::GetClosure(int addr, bool *ok) {
    auto ob = GetObject(addr);
    
    if (!ob->IsClosure()) {
        *ok = false;
        return make_handle<MIOClosure>(nullptr);
    }
    return make_handle(ob->AsClosure());
}

inline Handle<MIOVector> Thread::GetVector(int addr, bool *ok) {
    auto ob = GetObject(addr);

    if (!ob->IsVector()) {
        *ok = false;
        return make_handle<MIOVector>(nullptr);
    }
    return make_handle(ob->AsVector());
}

inline Handle<MIOHashMap> Thread::GetHashMap(int addr, bool *ok) {
    auto ob = GetObject(addr);

    if (!ob->IsHashMap()) {
        *ok = false;
        return make_handle<MIOHashMap>(nullptr);
    }
    return make_handle(ob->AsHashMap());
}

inline MIOGeneratedFunction *Thread::generated_function() {
    auto fn = callee_->AsGeneratedFunction();
    return fn ? fn : DCHECK_NOTNULL(callee_->AsClosure())->GetFunction()->AsGeneratedFunction();
}

inline mio_buf_t<uint8_t> Thread::const_primitive_buf() {
    return DCHECK_NOTNULL(generated_function())->GetConstantPrimitiveBuf();
}

inline mio_buf_t<HeapObject *> Thread::const_object_buf() {
    return DCHECK_NOTNULL(generated_function())->GetConstantObjectBuf();
}

inline mio_buf_t<UpValDesc> Thread::upvalue_buf() {
    auto closure = DCHECK_NOTNULL(callee_->AsClosure());
    DCHECK(closure->IsClose());
    return closure->GetUpValuesBuf();
}

inline FunctionDebugInfo *Thread::debug_info() {
    return DCHECK_NOTNULL(generated_function())->GetDebugInfo();
}

} // namespace mio

#endif // MIO_VM_THREAD_H_
