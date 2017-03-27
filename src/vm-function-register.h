#ifndef VM_FUNCTION_REGISTER_H_
#define VM_FUNCTION_REGISTER_H_

#include "vm-objects.h"
#include "code-label.h"
#include "base.h"

namespace mio {

class FunctionEntry {
public:
    DEF_PROP_RW(int, offset)
    DEF_PROP_RW(bool, is_native)
    DEF_MUTABLE_GETTER(CodeLabel, label)

    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionEntry)
private:
    int offset_;
    CodeLabel label_;
    bool is_native_;
}; // class FunctionEntry

class FunctionRegister {
public:
    FunctionRegister() = default;
    virtual ~FunctionRegister() = default;

    virtual FunctionEntry *FindOrInsert(const char *name) = 0;

    virtual bool RegisterNativeFunction(const char *name,
                                        MIOFunctionPrototype pointer) = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionRegister)
}; // class FunctionRegister

} // namespace mio

#endif // VM_FUNCTION_REGISTER_H_
