#ifndef MIO_VM_THREAD_H_
#define MIO_VM_THREAD_H_

#include "handles.h"
#include "base.h"

namespace mio {

class VM;
class Stack;
class HeapObject;

struct CallContext {
    int p_stack_base;
    int p_stack_size;
    int o_stack_base;
    int o_stack_size;
    int pc;
    uint64_t *bc;
};

class CallStack;
class MIOString;
class MIOError;
class MIOUnion;
class MIONormalFunction;
class MIOReflectionType;
class MIOReflectionIntegral;
class MIOReflectionFloating;

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

    mio_bool_t GetBool(int addr) { return GetI8(addr); }
    mio_i8_t   GetI8(int addr);
    mio_i16_t  GetI16(int addr);
    mio_i32_t  GetI32(int addr);
    mio_int_t  GetInt(int addr) { return GetI64(addr); }
    mio_i64_t  GetI64(int addr);

    mio_f32_t  GetF32(int addr);
    mio_f64_t  GetF64(int addr);

    Handle<HeapObject> GetObject(int addr);
    Handle<MIOString>  GetString(int addr, bool *ok);
    Handle<MIOError>   GetError(int addr, bool *ok);
    Handle<MIOUnion>   GetUnion(int addr, bool *ok);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Thread)
private:
    void ProcessObjectOperation(int id, uint16_t result, int16_t val1, int16_t val2, bool *ok);

    int ToString(TextOutputStream *stream, void *addr, Handle<MIOReflectionType> reflection, bool *ok);
    Handle<MIOUnion> CreateOrMergeUnion(int inbox, Handle<MIOReflectionType> reflection, bool *ok);
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

} // namespace mio

#endif // MIO_VM_THREAD_H_
