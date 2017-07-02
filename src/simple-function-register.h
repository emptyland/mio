#ifndef MIO_SIMPLE_FUNCTION_REGISTER_H_
#define MIO_SIMPLE_FUNCTION_REGISTER_H_

#include "vm-function-register.h"
#include "vm-objects.h"
#include "glog/logging.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace mio {

class MemorySegment;
class FunctionEntry;

class SimpleFunctionRegister : public FunctionRegister {
public:
    SimpleFunctionRegister(CodeCache *code_cache, MemorySegment *global)
        : FunctionRegister(code_cache)
        , global_(DCHECK_NOTNULL(global)) {}
    virtual ~SimpleFunctionRegister() override;

    virtual FunctionEntry *FindOrInsert(const char *name) override;

    virtual FunctionEntry *FindOrNull(const char *name) const override;

    virtual Handle<MIONativeFunction> FindNativeFunction(const char *name) override;

    virtual
    int GetAllFunctions(std::vector<Handle<MIOGeneratedFunction>> *all_functions) override;

private:
    int GetAllFnByFn(Handle<MIOGeneratedFunction> fn,
                     std::vector<Handle<MIOGeneratedFunction>> *all_functions,
                     std::unordered_set<MIOGeneratedFunction*> *unique_fn);

    std::unordered_map<std::string, FunctionEntry *> functions_;
    MemorySegment *global_;
};

} // namespace mio

#endif // MIO_SIMPLE_FUNCTION_REGISTER_H_
