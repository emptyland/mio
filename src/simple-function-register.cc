#include "simple-function-register.h"
#include "vm-memory-segment.h"

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
bool SimpleFunctionRegister::RegisterNativeFunction(const char *name,
                                                    MIOFunctionPrototype pointer) {
    auto iter = functions_.find(name);
    if (iter == functions_.end()) {
        return false;
    }

    auto entry = iter->second;
    if (entry->is_native()) {
        auto heap_obj = static_cast<HeapObject *>(global_->offset(entry->offset()));
        auto func = make_local(heap_obj->AsNativeFunction());
        DCHECK(!func.empty());

        func->SetNativePointer(pointer);
        return true;
    }
    return false;
}

int SimpleFunctionRegister::MakeFunctionInfo(FunctionInfoMap *map) {
    int result = 0;
    for (const auto &pair : functions_) {
        auto heap_obj = *static_cast<HeapObject **>(global_->offset(pair.second->offset()));

        auto func = make_local(heap_obj->AsNormalFunction());
        if (func.empty()) {
            continue;
        }

        map->emplace(func->GetAddress(), std::make_tuple(pair.first, pair.second));
        result++;
    }
    return result;
}

} // namespace mio
