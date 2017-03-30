#ifndef MIO_SIMPLE_FUNCTION_REGISTER_H_
#define MIO_SIMPLE_FUNCTION_REGISTER_H_

#include "vm-function-register.h"
#include "vm-objects.h"
#include "glog/logging.h"
#include <vector>
#include <unordered_map>

namespace mio {

class MemorySegment;
class FunctionEntry;

class SimpleFunctionRegister : public FunctionRegister {
public:
    SimpleFunctionRegister(MemorySegment *global)
        : global_(DCHECK_NOTNULL(global)) {}

    virtual ~SimpleFunctionRegister() override;

    virtual FunctionEntry *FindOrInsert(const char *name) override;

    virtual FunctionEntry *FindOrNull(const char *name) const override;

    virtual bool RegisterNativeFunction(const char *name,
                                        MIOFunctionPrototype pointer) override;

    int GetAllFunctions(std::vector<Local<MIONormalFunction>> *all_functions);

private:
    std::unordered_map<std::string, FunctionEntry *> functions_;
    MemorySegment *global_;
};

} // namespace mio

#endif // MIO_SIMPLE_FUNCTION_REGISTER_H_
