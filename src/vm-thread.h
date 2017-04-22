#ifndef MIO_VM_THREAD_H_
#define MIO_VM_THREAD_H_

#include "vm-objects.h"
#include "vm-stack.h"
#include "handles.h"
#include "base.h"

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
    };

    Thread(VM *vm);
    ~Thread();

    DEF_GETTER(ExitCode, exit_code)
    Stack *p_stack() const { return p_stack_; }
    Stack *o_stack() const { return o_stack_; }

    void Execute(MIONormalFunction *callee, bool *ok);

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
    inline Handle<MIOHashMap> GetHashMap(int addr, bool *ok);

    int GetCallStack(std::vector<MIOFunction *> *call_stack);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Thread)
private:
    void ProcessLoadPrimitive(CallContext *top, int bytes, uint16_t dest,
                              uint16_t segment, int32_t offset, bool *ok);
    void ProcessStorePrimitive(CallContext *top, int bytes, uint16_t addr,
                               uint16_t segment, int dest, bool *ok);
    void ProcessLoadObject(CallContext *top, uint16_t dest, uint16_t segment,
                           int32_t offset, bool *ok);
    void ProcessStoreObject(CallContext *top, uint16_t addr, uint16_t segment,
                            int dest, bool *ok);
    void ProcessObjectOperation(int id, uint16_t result, int16_t val1,
                                int16_t val2, bool *ok);

    int ToString(TextOutputStream *stream, void *addr,
                 Handle<MIOReflectionType> reflection, bool *ok);

    Handle<MIOUnion> CreateOrMergeUnion(int inbox,
                                        Handle<MIOReflectionType> reflection,
                                        bool *ok);
    void CreateEmptyValue(int result,
                          Handle<MIOReflectionType> reflection, bool *ok);
    Handle<MIOReflectionType> GetTypeInfo(int index);

    VM *vm_;
    Stack *p_stack_;
    Stack *o_stack_;
    CallStack *call_stack_;
    int pc_ = 0;
    uint64_t *bc_ = nullptr;
    bool should_exit_ = false;
    ExitCode exit_code_ = SUCCESS;
}; // class Thread


////////////////////////////////////////////////////////////////////////////////
/// Inline Functions:
////////////////////////////////////////////////////////////////////////////////

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

    if (!ob->IsUnion()) {
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

inline Handle<MIOHashMap> Thread::GetHashMap(int addr, bool *ok) {
    auto ob = GetObject(addr);

    if (!ob->IsHashMap()) {
        *ok = false;
        return make_handle<MIOHashMap>(nullptr);
    }
    return make_handle(ob->AsHashMap());
}

} // namespace mio

#endif // MIO_VM_THREAD_H_
