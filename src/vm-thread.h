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
class MIONormalFunction;

class Thread {
public:
    enum ExitCode: int {
        SUCCESS,
        DEBUGGING,
        PANIC,
        STACK_OVERFLOW,
        NULL_NATIVE_FUNCTION,
    };

    Thread(VM *vm);
    ~Thread();

    DEF_GETTER(ExitCode, exit_code)
    Stack *p_stack() const { return p_stack_; }
    Stack *o_stack() const { return o_stack_; }

    void Execute(MIONormalFunction *callee, bool *ok);

    Local<HeapObject> GetObject(int addr);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Thread)
private:
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
