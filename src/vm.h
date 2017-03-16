#ifndef MIO_VM_H_
#define MIO_VM_H_

#include "base.h"
#include <map>

namespace mio {

class Thread;
class MemorySegment;

class VM {
public:

    friend class Thread;

    DISALLOW_IMPLICIT_CONSTRUCTORS(VM)
private:
    Thread *main_thread_;
    MemorySegment *code_;
    //std::map<std::string, FunctionEntry*> function_symbols_;
};

} // namespace mio

#endif // MIO_VM_H_
