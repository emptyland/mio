#ifndef MIO_VM_THREAD_H_
#define MIO_VM_THREAD_H_

#include "base.h"

namespace mio {

class VM;
class Stack;

class Thread {
public:
    enum ExitCode: int {
        SUCCESS,
        DEBUGGING,

    };

    Thread(VM *vm);
    ~Thread();

    void Execute(int pc, bool *ok);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Thread)
private:
    VM *vm_;
    Stack *p_stack_;
    Stack *o_stack_;
    int pc_ = 0;
    bool should_exit_ = false;
    ExitCode exit_code_ = SUCCESS;
}; // class Thread

} // namespace mio

#endif // MIO_VM_THREAD_H_
