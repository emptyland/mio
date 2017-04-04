#ifndef VM_FUNCTION_REGISTER_H_
#define VM_FUNCTION_REGISTER_H_

#include "vm-objects.h"
#include "base.h"
#include <vector>

namespace mio {

class FunctionEntry;

class FunctionRegister {
public:
    FunctionRegister() = default;
    virtual ~FunctionRegister() = default;

    virtual FunctionEntry *FindOrInsert(const char *name) = 0;

    virtual FunctionEntry *FindOrNull(const char *name) const = 0;

    virtual bool RegisterNativeFunction(const char *name,
                                        MIOFunctionPrototype pointer) = 0;

    virtual int GetAllFunctions(std::vector<Local<MIONormalFunction>> *all_functions) = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionRegister)
}; // class FunctionRegister

} // namespace mio

#endif // VM_FUNCTION_REGISTER_H_
