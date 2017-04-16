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
    auto ob = static_cast<MIOString *>(malloc(total_size));
    ob->SetKind(HeapObject::kString);
    objects_.push_back(ob);

    ob->SetLength(payload_length);
    auto p = ob->mutable_data();
    for (int i = 0; i < n; ++i) {
        memcpy(p, bufs[i].z, bufs[i].n);
        p += bufs[i].n;
    }
    ob->mutable_data()[payload_length] = '\0';
    return make_handle(ob);
}

/*virtual*/
Handle<MIOClosure>
MallocedObjectFactory::CreateClosure(Handle<MIOFunction> function,
                                     int up_values_size) {
    auto placement_size = MIOClosure::kUpValesOffset +
            up_values_size * sizeof(UpValDesc);
    auto ob = static_cast<MIOClosure *>(::malloc(placement_size));
    ob->SetKind(HeapObject::kClosure);
    objects_.push_back(ob);

    ob->SetFlags(0);
    ob->SetFunction(function.get());
    ob->SetUpValueSize(up_values_size);
    return make_handle(ob);
}

/*virtual*/
Handle<MIONativeFunction>
MallocedObjectFactory::CreateNativeFunction(const char *signature,
                                            MIOFunctionPrototype pointer) {
    Handle<MIOString> sign(ObjectFactory::CreateString(signature,
                                                       static_cast<int>(strlen(signature))));
    NEW_MIO_OBJECT(ob, NativeFunction);
    ob->SetSignature(sign.get());
    ob->SetNativePointer(pointer);
    return make_handle(ob);
}

/*virtual*/
Handle<MIONormalFunction>
MallocedObjectFactory::CreateNormalFunction(const std::vector<Handle<HeapObject>> &constant_objects,
                                            const void *constant_primitive_data,
                                            int constant_primitive_size,
                                            const void *code,
                                            int code_size) {
    DCHECK_EQ(0, code_size % sizeof(uint64_t));

    auto placement_size = MIONormalFunction::kHeaderOffset +
            constant_primitive_size +
            constant_objects.size() * kObjectReferenceSize + code_size;
    auto ob = static_cast<MIONormalFunction *>(malloc(placement_size));
    ob->SetKind(HeapObject::kNormalFunction);
    objects_.push_back(ob);

    ob->SetName(nullptr);

    ob->SetConstantPrimitiveSize(constant_primitive_size);
    memcpy(ob->GetConstantPrimitiveData(), constant_primitive_data, constant_primitive_size);

    ob->SetConstantObjectSize(static_cast<int>(constant_objects.size()));
    for (int i = 0; i < ob->GetConstantObjectSize(); ++i) {
        ob->GetConstantObjects()[i] = constant_objects[i].get();
    }

    ob->SetCodeSize(code_size / sizeof(uint64_t));
    memcpy(ob->GetCode(), code, code_size);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOHashMap> MallocedObjectFactory::CreateHashMap(int seed, uint32_t flags) {
    NEW_MIO_OBJECT(ob, HashMap);
    ob->SetSeed(seed);
    ob->SetSize(0);
    ob->SetFlags(flags);
    // TODO: slots

    return make_handle(ob);
}

/*virtual*/
Handle<MIOError>
MallocedObjectFactory::CreateError(const char *message, int position,
                                   Handle<MIOError> linked) {
    NEW_MIO_OBJECT(ob, Error);
    ob->SetPosition(position);
    ob->SetMessage(GetOrNewString(message, static_cast<int>(strlen(message)),
                                   nullptr).get());
    ob->SetLinkedError(linked.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOUnion>
MallocedObjectFactory::CreateUnion(const void *data, int size,
                                   Handle<MIOReflectionType> type_info) {
    DCHECK_GE(size, 0);
    DCHECK_LE(size, kMaxReferenceValueSize);

    NEW_MIO_OBJECT(ob, Union);
    ob->SetTypeInfo(type_info.get());
    if (size > 0) {
        memcpy(ob->mutable_data(), data, size);
    }
    return make_handle(ob);
}

/*virtual*/
Handle<MIOUpValue>
MallocedObjectFactory::GetOrNewUpValue(const void *data, int size,
                                       int32_t unique_id, bool is_primitive) {
    auto iter = upvalues_.find(unique_id);
    if (iter != upvalues_.end()) {
        return make_handle(iter->second);
    }

    auto placement_size = MIOUpValue::kHeaderOffset + size;
    auto ob = static_cast<MIOUpValue *>(::malloc(placement_size));
    ob->SetKind(HeapObject::kUpValue);
    objects_.push_back(ob);

    ob->SetFlags((unique_id << 1) | (is_primitive ? 0x0 : 0x1));
    ob->SetValueSize(size);
    memcpy(ob->GetValue(), data, size);

    upvalues_.emplace(unique_id, ob);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionVoid>
MallocedObjectFactory::CreateReflectionVoid(int64_t tid) {
    NEW_MIO_OBJECT(ob, ReflectionVoid);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionIntegral>
MallocedObjectFactory::CreateReflectionIntegral(int64_t tid, int bitwide) {
    NEW_MIO_OBJECT(ob, ReflectionIntegral);
    ob->SetTid(tid);
    ob->SetReferencedSize((bitwide + 7) / 8);
    ob->SetBitWide(bitwide);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionFloating>
MallocedObjectFactory::CreateReflectionFloating(int64_t tid, int bitwide) {
    NEW_MIO_OBJECT(ob, ReflectionFloating);
    ob->SetTid(tid);
    ob->SetReferencedSize((bitwide + 7) / 8);
    ob->SetBitWide(bitwide);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionString>
MallocedObjectFactory::CreateReflectionString(int64_t tid) {
    NEW_MIO_OBJECT(ob, ReflectionString);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionError>
MallocedObjectFactory::CreateReflectionError(int64_t tid) {
    NEW_MIO_OBJECT(ob, ReflectionError);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionUnion>
MallocedObjectFactory::CreateReflectionUnion(int64_t tid) {
    NEW_MIO_OBJECT(ob, ReflectionUnion);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionMap>
MallocedObjectFactory::CreateReflectionMap(int64_t tid,
                                           Handle<MIOReflectionType> key,
                                           Handle<MIOReflectionType> value) {
    NEW_MIO_OBJECT(ob, ReflectionMap);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetKey(key.get());
    ob->SetValue(value.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionFunction>
MallocedObjectFactory::CreateReflectionFunction(int64_t tid, Handle<MIOReflectionType> return_type,
                                                int number_of_parameters,
                                                const std::vector<Handle<MIOReflectionType>> &parameters) {
    int placement_size = MIOReflectionFunction::kParamtersOffset +
            sizeof(MIOReflectionType *) * number_of_parameters;

    auto ob = static_cast<MIOReflectionFunction *>(malloc(placement_size));
    objects_.push_back(ob);

    ob->SetKind(HeapObject::kReflectionFunction);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetNumberOfParameters(number_of_parameters);
    ob->SetReturn(return_type.get());

    for (size_t i = 0; i < parameters.size(); ++i) {
        ob->GetParamters()[i] = parameters[i].get();
    }
    return make_handle(ob);
}

} // namespace mio
