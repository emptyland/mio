#ifndef MIO_VM_H_
#define MIO_VM_H_

#include "base.h"
#include "handles.h"
#include <map>

namespace mio {

class VM;
class Thread;
class MemorySegment;
class MIOString;
class MIONativeFunction;
class MIONormalFunction;
class MIOHashMap;

typedef int (*MIOFunctionPrototype)(VM *, Thread *);

class VM {
public:
    VM();
    ~VM();

    DEF_GETTER(int, max_call_deep)

    Thread *TEST_main_thread() const { return main_thread_; }
    MemorySegment *TEST_code() const { return code_; }

    friend class Thread;
    DISALLOW_IMPLICIT_CONSTRUCTORS(VM)
private:
    int max_call_deep_ = kDefaultMaxCallDeep;
    Thread *main_thread_;
    MemorySegment *code_;
};

} // namespace mio

#endif // MIO_VM_H_
