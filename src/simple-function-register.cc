#include "simple-function-register.h"
#include "vm-memory-segment.h"
#include "compiler.h"

namespace mio {

/*virtual*/ SimpleFunctionRegister::~SimpleFunctionRegister() {
    for (const auto &pair : functions_) {
        delete pair.second;
    }
}

/*virtual*/
FunctionEntry *SimpleFunctionRegister::FindOrInsert(const char *name) {
    FunctionEntry *entry = nullptr;

    auto iter = functions_.find(name);
    if (iter == functions_.end()) {
        entry = new FunctionEntry;
        functions_.insert(std::make_pair(name, entry));
    } else {
        entry = iter->second;
    }
    return entry;
}

/*virtual*/
FunctionEntry *SimpleFunctionRegister::FindOrNull(const char *name) const {
    auto iter = functions_.find(name);
    return iter == functions_.end() ? nullptr : iter->second;
}

/*virtual*/
Handle<MIONativeFunction>
SimpleFunctionRegister::FindNativeFunction(const char *name) {
    auto iter = functions_.find(name);
    if (iter == functions_.end()) {
        return Handle<MIONativeFunction>();
    }

    auto entry = iter->second;
    if (entry->kind() == FunctionEntry::NATIVE) {
        auto heap_obj = global_->Get<HeapObject *>(entry->offset());
        auto func = make_handle(heap_obj->AsNativeFunction());
        DCHECK(!func.empty());
        return func;
    }
    return Handle<MIONativeFunction>();
}

int
SimpleFunctionRegister::GetAllFunctions(std::vector<Handle<MIONormalFunction>> *all_functions) {
    int result = 0;
    std::unordered_set<MIONormalFunction*> unique_fn;
    for (const auto &pair : functions_) {
        auto obj = global_->Get<HeapObject *>(pair.second->offset());
        auto fn = make_handle(obj->AsNormalFunction());

        if (!fn.empty() && unique_fn.find(fn.get()) == unique_fn.end()) {
            all_functions->push_back(fn);
            unique_fn.insert(fn.get());
            result += GetAllFnByFn(fn, all_functions, &unique_fn) + 1;
        }

    }
    return result;
}

int SimpleFunctionRegister::GetAllFnByFn(Handle<MIONormalFunction> fn,
                                         std::vector<Handle<MIONormalFunction>> *all_functions,
                                         std::unordered_set<MIONormalFunction*> *unique_fn) {
    int result = 0;
    for (int i = 0; i < fn->GetConstantObjectSize(); ++i) {
        if (fn->GetConstantObject(i)->IsClosure()) {
            auto closure = fn->GetConstantObject(i)->AsClosure();
            auto core = make_handle(closure->GetFunction());
            if (core->IsNormalFunction()) {
                auto core_fn = make_handle(core->AsNormalFunction());
                if (unique_fn->find(core_fn.get()) == unique_fn->end()) {
                    all_functions->push_back(core_fn);
                    unique_fn->insert(core_fn.get());
                    result += GetAllFnByFn(core_fn, all_functions, unique_fn) + 1;
                }
            }
        } else if (fn->GetConstantObject(i)->IsNormalFunction()) {
            auto core = make_handle(fn->GetConstantObject(i)->AsNormalFunction());
            if (unique_fn->find(core.get()) == unique_fn->end()) {
                all_functions->push_back(core);
                unique_fn->insert(core.get());
                result += GetAllFnByFn(core, all_functions, unique_fn) + 1;
            }
        }
    }
    return result;
}

} // namespace mio
