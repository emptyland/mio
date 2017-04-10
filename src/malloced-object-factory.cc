#include "malloced-object-factory.h"
#include "vm-objects.h"
#include "glog/logging.h"
#include <stdlib.h>

namespace mio {

#define NEW_MIO_OBJECT(obj, name) \
    auto obj = static_cast<MIO##name *>(malloc(MIO##name::kMIO##name##Offset)); \
    obj->SetKind(HeapObject::k##name); \
    objects_.push_back(obj)

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
Handle<MIOString>
MallocedObjectFactory::GetOrNewString(const char *z, int n, int **offset) {
    if (offset) {
        offset_stub_ = -1;
        *offset = offset_pointer_;
    }
    return ObjectFactory::CreateString(z, n);
}

/*virtual*/
Handle<MIOString> MallocedObjectFactory::CreateString(const mio_strbuf_t *bufs, int n) {
    auto payload_length = 0;
    DCHECK_GE(n, 0);
    for (int i = 0; i < n; ++i) {
        payload_length += bufs[i].n;
    }

    auto total_size = payload_length + 1 + MIOString::kDataOffset; // '\0' + MIOStringHeader
    auto obj = static_cast<MIOString *>(malloc(total_size));
    obj->SetKind(HeapObject::kString);
    objects_.push_back(obj);

    obj->SetLength(payload_length);
    auto p = obj->mutable_data();
    for (int i = 0; i < n; ++i) {
        memcpy(p, bufs[i].z, bufs[i].n);
        p += bufs[i].n;
    }
    obj->mutable_data()[payload_length] = '\0';
    return make_handle(obj);
}

/*virtual*/
Handle<MIONativeFunction> MallocedObjectFactory::CreateNativeFunction(const char *signature,
                                                  MIOFunctionPrototype pointer) {
    Handle<MIOString> sign(ObjectFactory::CreateString(signature,
                                                       static_cast<int>(strlen(signature))));
    NEW_MIO_OBJECT(obj, NativeFunction);
    obj->SetSignature(sign.get());
    obj->SetNativePointer(pointer);
    return make_handle(obj);
}

/*virtual*/
Handle<MIONormalFunction> MallocedObjectFactory::CreateNormalFunction(const void *code, int size) {
    DCHECK_EQ(0, size % sizeof(uint64_t));

    auto obj = static_cast<MIONormalFunction *>(malloc(MIONormalFunction::kHeadOffset + size));
    obj->SetKind(HeapObject::kNormalFunction);
    objects_.push_back(obj);

    obj->SetName(ObjectFactory::CreateString("", 0).get());
    obj->SetCodeSize(size / 8);
    memcpy(obj->GetCode(), code, size);
    return make_handle(obj);
}

/*virtual*/
Handle<MIOHashMap> MallocedObjectFactory::CreateHashMap(int seed, uint32_t flags) {
    NEW_MIO_OBJECT(obj, HashMap);
    obj->SetSeed(seed);
    obj->SetSize(0);
    obj->SetFlags(flags);
    // TODO: slots

    return make_handle(obj);
}

/*virtual*/
Handle<MIOError>
MallocedObjectFactory::CreateError(const char *message, int position,
                                   Handle<MIOError> linked) {
    NEW_MIO_OBJECT(obj, Error);
    obj->SetPosition(position);
    obj->SetMessage(GetOrNewString(message, static_cast<int>(strlen(message)),
                                   nullptr).get());
    obj->SetLinkedError(linked.get());
    return make_handle(obj);
}

/*virtual*/
Handle<MIOUnion>
MallocedObjectFactory::CreateUnion(const void *data, int size,
                                   Handle<MIOReflectionType> type_info) {
    DCHECK_GE(size, 0);
    DCHECK_LE(size, kMaxReferenceValueSize);

    NEW_MIO_OBJECT(obj, Union);
    obj->SetTypeInfo(type_info.get());
    if (size > 0) {
        memcpy(obj->mutable_data(), data, size);
    }
    return make_handle(obj);
}

/*virtual*/
Handle<MIOReflectionVoid>
MallocedObjectFactory::CreateReflectionVoid(int64_t tid) {
    NEW_MIO_OBJECT(obj, ReflectionVoid);
    obj->SetTid(tid);
    obj->SetReferencedSize(kObjectReferenceSize);
    return make_handle(obj);
}

/*virtual*/
Handle<MIOReflectionIntegral>
MallocedObjectFactory::CreateReflectionIntegral(int64_t tid, int bitwide) {
    NEW_MIO_OBJECT(obj, ReflectionIntegral);
    obj->SetTid(tid);
    obj->SetReferencedSize((bitwide + 7) / 8);
    obj->SetBitWide(bitwide);
    return make_handle(obj);
}

/*virtual*/
Handle<MIOReflectionFloating>
MallocedObjectFactory::CreateReflectionFloating(int64_t tid, int bitwide) {
    NEW_MIO_OBJECT(obj, ReflectionFloating);
    obj->SetTid(tid);
    obj->SetReferencedSize((bitwide + 7) / 8);
    obj->SetBitWide(bitwide);
    return make_handle(obj);
}

/*virtual*/
Handle<MIOReflectionString>
MallocedObjectFactory::CreateReflectionString(int64_t tid) {
    NEW_MIO_OBJECT(obj, ReflectionString);
    obj->SetTid(tid);
    obj->SetReferencedSize(kObjectReferenceSize);
    return make_handle(obj);
}

/*virtual*/
Handle<MIOReflectionError>
MallocedObjectFactory::CreateReflectionError(int64_t tid) {
    NEW_MIO_OBJECT(obj, ReflectionError);
    obj->SetTid(tid);
    obj->SetReferencedSize(kObjectReferenceSize);
    return make_handle(obj);
}

/*virtual*/
Handle<MIOReflectionUnion>
MallocedObjectFactory::CreateReflectionUnion(int64_t tid) {
    NEW_MIO_OBJECT(obj, ReflectionUnion);
    obj->SetTid(tid);
    obj->SetReferencedSize(kObjectReferenceSize);
    return make_handle(obj);
}

/*virtual*/
Handle<MIOReflectionMap>
MallocedObjectFactory::CreateReflectionMap(int64_t tid,
                                           Handle<MIOReflectionType> key,
                                           Handle<MIOReflectionType> value) {
    NEW_MIO_OBJECT(obj, ReflectionMap);
    obj->SetTid(tid);
    obj->SetReferencedSize(kObjectReferenceSize);
    obj->SetKey(key.get());
    obj->SetValue(value.get());
    return make_handle(obj);
}

/*virtual*/
Handle<MIOReflectionFunction>
MallocedObjectFactory::CreateReflectionFunction(int64_t tid, Handle<MIOReflectionType> return_type,
                                                int number_of_parameters,
                                                const std::vector<Handle<MIOReflectionType>> &parameters) {
    int placement_size = MIOReflectionFunction::kParamtersOffset +
            sizeof(MIOReflectionType *) * number_of_parameters;

    auto obj = static_cast<MIOReflectionFunction *>(malloc(placement_size));
    objects_.push_back(obj);

    obj->SetKind(HeapObject::kReflectionFunction);
    obj->SetTid(tid);
    obj->SetReferencedSize(kObjectReferenceSize);
    obj->SetNumberOfParameters(number_of_parameters);
    obj->SetReturn(return_type.get());

    for (size_t i = 0; i < parameters.size(); ++i) {
        obj->GetParamters()[i] = parameters[i].get();
    }
    return make_handle(obj);
}

} // namespace mio
