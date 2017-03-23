#include "vm.h"
#include "vm-memory-segment.h"
#include "vm-thread.h"
#include "vm-objects.h"

namespace mio {

VM::VM()
    : main_thread_(new Thread(this))
    , code_(new MemorySegment()) {
}

VM::~VM() {
    delete code_;
    delete main_thread_;
}

Local<MIOString> VM::CreateString(const char *z, int n) {
    // TODO:
    auto total_size = n + 1 + MIOString::kDataOffset;
    auto obj = static_cast<MIOString *>(malloc(total_size));
    obj->SetKind(HeapObject::kString);
    obj->SetLength(n);
    memcpy(obj->mutable_data(), z, n);
    obj->mutable_data()[n] = '\0';

    return make_local(obj);
}

Local<MIONativeFunction> VM::CreateNativeFunction(const char *signature,
                                                  MIOFunctionPrototype pointer) {
    // TODO:
    Local<MIOString> sign(CreateString(signature,
                                       static_cast<int>(strlen(signature))));
    auto obj = static_cast<MIONativeFunction *>(malloc(MIONativeFunction::kMIONativeFunctionOffset));
    obj->SetKind(HeapObject::kNativeFunction);
    obj->SetSignature(sign.get());
    obj->SetNativePointer(pointer);
    return make_local(obj);
}

Local<MIONormalFunction> VM::CreateNormalFunction(int address) {
    // TODO:
    auto obj = static_cast<MIONormalFunction *>(malloc(MIONormalFunction::kMIONormalFunctionOffset));
    obj->SetKind(HeapObject::kNormalFunction);
    obj->SetAddress(address);
    return make_local(obj);
}

Local<MIOHashMap> VM::CreateHashMap(int seed, uint32_t flags) {
    // TODO:
    auto obj = static_cast<MIOHashMap *>(malloc(MIOHashMap::kMIOHashMapOffset));
    obj->SetKind(HeapObject::kHashMap);
    obj->SetSeed(seed);
    obj->SetSize(0);
    obj->SetFlags(flags);
    obj->SetSlots(static_cast<MapPair*>(malloc(MapPair::kPayloadOffset * MIOHashMap::kDefaultInitialSlots)));
    return make_local(obj);
}

} // namespace mio
