#ifndef MIO_SIMPLE_FUNCTION_REGISTER_H_
#define MIO_SIMPLE_FUNCTION_REGISTER_H_

#include "vm-function-register.h"
#include "compiler.h"

namespace mio {

class MemorySegment;

class SimpleFunctionRegister : public FunctionRegister {
public:
    SimpleFunctionRegister(MemorySegment *global)
        : global_(DCHECK_NOTNULL(global)) {}

    virtual ~SimpleFunctionRegister() override;

    virtual FunctionEntry *FindOrInsert(const char *name) override;

    virtual bool RegisterNativeFunction(const char *name,
                                        MIOFunctionPrototype pointer) override;

    int MakeFunctionInfo(FunctionInfoMap *map);

private:
    std::unordered_map<std::string, FunctionEntry *> functions_;
    MemorySegment *global_;
};

} // namespace mio

#endif // MIO_SIMPLE_FUNCTION_REGISTER_H_
