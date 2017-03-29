#include "malloced-object-factory.h"
#include "vm-objects.h"
#include <stdlib.h>

namespace mio {

MallocedObjectFactory::MallocedObjectFactory()
    : offset_stub_(0)
    , offset_pointer_(&offset_stub_) {}

/*virtual*/
MallocedObjectFactory::~MallocedObjectFactory() {
    for (auto obj : objects_) {
        free(obj);
    }
}

/*virtual*/
Local<MIOString>
MallocedObjectFactory::GetOrNewString(const char *z, int n, int **offset) {
    offset_stub_ = -1;
    *offset = offset_pointer_;
    return CreateString(z, n);
}

/*virtual*/
Local<MIOString> MallocedObjectFactory::CreateString(const char *z, int n) {
    auto total_size = n + 1 + MIOString::kDataOffset;
    auto obj = static_cast<MIOString *>(malloc(total_size));
    obj->SetKind(HeapObject::kString);
    obj->SetLength(n);
    memcpy(obj->mutable_data(), z, n);
    obj->mutable_data()[n] = '\0';

    objects_.push_back(obj);
    return make_local(obj);
}

/*virtual*/
Local<MIONativeFunction> MallocedObjectFactory::CreateNativeFunction(const char *signature,
                                                  MIOFunctionPrototype pointer) {
    Local<MIOString> sign(CreateString(signature,
                                       static_cast<int>(strlen(signature))));
    auto obj = static_cast<MIONativeFunction *>(malloc(MIONativeFunction::kMIONativeFunctionOffset));
    obj->SetKind(HeapObject::kNativeFunction);
    obj->SetSignature(sign.get());
    obj->SetNativePointer(pointer);

    objects_.push_back(obj);
    return make_local(obj);
}

/*virtual*/
Local<MIONormalFunction> MallocedObjectFactory::CreateNormalFunction(int address) {
    auto obj = static_cast<MIONormalFunction *>(malloc(MIONormalFunction::kMIONormalFunctionOffset));
    obj->SetKind(HeapObject::kNormalFunction);
    obj->SetAddress(address);

    objects_.push_back(obj);
    return make_local(obj);
}

/*virtual*/
Local<MIOHashMap> MallocedObjectFactory::CreateHashMap(int seed, uint32_t flags) {
    auto obj = static_cast<MIOHashMap *>(malloc(MIOHashMap::kMIOHashMapOffset));
    obj->SetKind(HeapObject::kHashMap);
    obj->SetSeed(seed);
    obj->SetSize(0);
    obj->SetFlags(flags);
    obj->SetSlots(static_cast<MapPair*>(malloc(MapPair::kPayloadOffset * MIOHashMap::kDefaultInitialSlots)));

    objects_.push_back(obj);
    return make_local(obj);
}


} // namespace mio
