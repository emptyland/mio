#include "do-nothing-garbage-collector.h"
#include "vm-object-surface.h"
#include "vm-objects.h"
#include "glog/logging.h"
#include <stdlib.h>

namespace mio {

#define NEW_OBJECT(name) NewObject<MIO##name>(MIO##name::kMIO##name##Offset);

DoNothingGarbageCollector::DoNothingGarbageCollector(ManagedAllocator *allocator)
    : allocator_(DCHECK_NOTNULL(allocator)) {
}

/*virtual*/
DoNothingGarbageCollector::~DoNothingGarbageCollector() {
    for (auto ob : objects_) {
        allocator_->Free(ob);
    }
}

/*virtual*/
Handle<MIOString> DoNothingGarbageCollector::GetOrNewString(const mio_strbuf_t *bufs, int n) {
    auto payload_length = 0;
    DCHECK_GE(n, 0);
    for (int i = 0; i < n; ++i) {
        payload_length += bufs[i].n;
    }

    auto total_size = payload_length + 1 + MIOString::kDataOffset; // '\0' + MIOStringHeader
    auto ob = NewObject<MIOString>(total_size);

    ob->SetLength(payload_length);
    auto p = ob->GetMutableData();
    for (int i = 0; i < n; ++i) {
        memcpy(p, bufs[i].z, bufs[i].n);
        p += bufs[i].n;
    }
    ob->GetMutableData()[payload_length] = '\0';
    return make_handle(ob);
}

/*virtual*/
Handle<MIOClosure>
DoNothingGarbageCollector::CreateClosure(Handle<MIOFunction> function,
                                     int up_values_size) {
    auto placement_size = static_cast<int>(MIOClosure::kUpValesOffset +
            up_values_size * sizeof(UpValDesc));

    auto ob = NewObject<MIOClosure>(placement_size);
    ob->SetFlags(0);
    ob->SetFunction(function.get());
    ob->SetUpValueSize(up_values_size);
    return make_handle(ob);
}

/*virtual*/
Handle<MIONativeFunction>
DoNothingGarbageCollector::CreateNativeFunction(const char *signature,
                                            MIOFunctionPrototype pointer) {
    Handle<MIOString> sign(ObjectFactory::GetOrNewString(signature,
                                                         static_cast<int>(strlen(signature))));
    auto ob = NEW_OBJECT(NativeFunction);
    ob->SetSignature(sign.get());
    ob->SetNativePointer(pointer);
    ob->SetNativeWarperIndex(nullptr);
    return make_handle(ob);
}

/*virtual*/
Handle<MIONormalFunction>
DoNothingGarbageCollector::CreateNormalFunction(const std::vector<Handle<HeapObject>> &constant_objects,
                                            const void *constant_primitive_data,
                                            int constant_primitive_size,
                                            const void *code,
                                            int code_size) {
    DCHECK_EQ(0, code_size % sizeof(uint64_t));

    auto placement_size = static_cast<int>(MIONormalFunction::kHeaderOffset +
            constant_primitive_size +
            constant_objects.size() * kObjectReferenceSize + code_size);

    auto ob = NewObject<MIONormalFunction>(placement_size);
    ob->SetName(nullptr);
    ob->SetDebugInfo(nullptr);

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
Handle<MIOVector>
DoNothingGarbageCollector::CreateVector(int initial_size,
                                        Handle<MIOReflectionType> element) {
    DCHECK_GE(initial_size, 0);
    DCHECK_NE(HeapObject::kReflectionVoid, element->GetKind());

    auto ob = NewObject<MIOVector>(MIOVector::kMIOVectorOffset);
    ob->SetSize(initial_size);
    ob->SetCapacity(initial_size < MIOVector::kMinCapacity
                    ? MIOVector::kMinCapacity
                    : initial_size * MIOVector::kCapacityScale);
    ob->SetElement(element.get());

    auto data = allocator_->Allocate(ob->GetCapacity() * element->GetTypePlacementSize());
    if (!data) {
        return Handle<MIOVector>();
    }
    if (element->IsObject()) {
        memset(data, 0, initial_size * element->GetTypePlacementSize());
    }
    ob->SetData(data);

    return make_handle(ob);
}

/*virtual*/
Handle<MIOSlice>
DoNothingGarbageCollector::CreateSlice(int begin, int size,
                                       Handle<HeapObject> input) {
    DCHECK(input->IsVector() || input->IsSlice());
    Handle<MIOVector> core;
    int current_begin = 0, current_size = 0;
    if (core->IsVector()) {
        current_begin = 0;
        current_size  = input->AsVector()->GetSize();
        core          = input->AsVector();
    } else {
        auto slice    = input->AsSlice();
        current_begin = slice->GetRangeBegin();
        current_size  = slice->GetRangeSize();
        core          = slice->GetVector();
    }

    DCHECK_GE(begin, 0);
    DCHECK_LT(begin, current_size);

    begin += current_begin;
    auto ob = NewObject<MIOSlice>(MIOSlice::kMIOSliceOffset);
    ob->SetRangeBegin(begin);

    auto remain = current_size - begin;
    if (size < 0) {
        ob->SetRangeSize(remain);
    } else {
        ob->SetRangeSize(size >= remain ? remain : size);
    }
    ob->SetVector(core.get());

    return make_handle(ob);
}

/*virtual*/
Handle<MIOHashMap>
DoNothingGarbageCollector::CreateHashMap(int seed, int initial_slots,
                                         Handle<MIOReflectionType> key,
                                         Handle<MIOReflectionType> value) {
    DCHECK(key->CanBeKey());
    auto ob = NewObject<MIOHashMap>(MIOHashMap::kMIOHashMapOffset);
    ob->SetSeed(seed);
    ob->SetKey(key.get());
    ob->SetValue(value.get());
    ob->SetSize(0);

    DCHECK_GE(initial_slots, 0);
    ob->SetSlotSize(initial_slots);
    if (ob->GetSlotSize() > 0) {
        auto slots_placement_size = static_cast<int>(sizeof(MIOHashMap::Slot) * ob->GetSlotSize());
        auto slots = static_cast<MIOHashMap::Slot *>(allocator_->Allocate(slots_placement_size));
        memset(slots, 0, slots_placement_size);
        ob->SetSlots(slots);
    }
    return make_handle(ob);
}

/*virtual*/
Handle<MIOError>
DoNothingGarbageCollector::CreateError(Handle<MIOString> msg, Handle<MIOString> file_name,
                                       int position, Handle<MIOError> linked) {
    auto ob = NEW_OBJECT(Error);
    ob->SetFileName(file_name.get());
    ob->SetPosition(position);
    ob->SetMessage(msg.get());
    ob->SetLinkedError(linked.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOUnion>
DoNothingGarbageCollector::CreateUnion(const void *data, int size,
                                   Handle<MIOReflectionType> type_info) {
    DCHECK_GE(size, 0);
    DCHECK_LE(size, kMaxReferenceValueSize);

    auto ob = NEW_OBJECT(Union);
    ob->SetTypeInfo(type_info.get());
    if (size > 0) {
        memcpy(ob->GetMutableData(), data, size);
    }
    return make_handle(ob);
}

/*virtual*/
Handle<MIOUpValue>
DoNothingGarbageCollector::GetOrNewUpValue(const void *data, int size,
                                       int32_t unique_id, bool is_primitive) {
    auto iter = upvalues_.find(unique_id);
    if (iter != upvalues_.end()) {
        return make_handle(iter->second);
    }

    auto placement_size = MIOUpValue::kHeaderOffset + size;
    auto ob = NewObject<MIOUpValue>(placement_size);
    ob->SetFlags((unique_id << 1) | (is_primitive ? 0x0 : 0x1));
    ob->SetValueSize(size);
    memcpy(ob->GetValue(), data, size);

    upvalues_.emplace(unique_id, ob);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionVoid>
DoNothingGarbageCollector::CreateReflectionVoid(int64_t tid) {
    auto ob = NEW_OBJECT(ReflectionVoid)
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionIntegral>
DoNothingGarbageCollector::CreateReflectionIntegral(int64_t tid, int bitwide) {
    auto ob = NEW_OBJECT(ReflectionIntegral);
    ob->SetTid(tid);
    ob->SetReferencedSize((bitwide + 7) / 8);
    ob->SetBitWide(bitwide);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionFloating>
DoNothingGarbageCollector::CreateReflectionFloating(int64_t tid, int bitwide) {
    auto ob = NEW_OBJECT(ReflectionFloating);
    ob->SetTid(tid);
    ob->SetReferencedSize((bitwide + 7) / 8);
    ob->SetBitWide(bitwide);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionString>
DoNothingGarbageCollector::CreateReflectionString(int64_t tid) {
    auto ob = NEW_OBJECT(ReflectionString);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionError>
DoNothingGarbageCollector::CreateReflectionError(int64_t tid) {
    auto ob = NEW_OBJECT(ReflectionError);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionUnion>
DoNothingGarbageCollector::CreateReflectionUnion(int64_t tid) {
    auto ob = NEW_OBJECT(ReflectionUnion);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionArray>
DoNothingGarbageCollector::CreateReflectionArray(int64_t tid,
                                           Handle<MIOReflectionType> element) {
    auto ob = NEW_OBJECT(ReflectionArray);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetElement(element.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionSlice>
DoNothingGarbageCollector::CreateReflectionSlice(int64_t tid,
                                           Handle<MIOReflectionType> element) {
    auto ob = NEW_OBJECT(ReflectionSlice);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetElement(element.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionMap>
DoNothingGarbageCollector::CreateReflectionMap(int64_t tid,
                                           Handle<MIOReflectionType> key,
                                           Handle<MIOReflectionType> value) {
    auto ob = NEW_OBJECT(ReflectionMap);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetKey(key.get());
    ob->SetValue(value.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionFunction>
DoNothingGarbageCollector::CreateReflectionFunction(int64_t tid, Handle<MIOReflectionType> return_type,
                                                    int number_of_parameters,
                                                    const std::vector<Handle<MIOReflectionType>> &parameters) {
    int placement_size = MIOReflectionFunction::kParamtersOffset +
            sizeof(MIOReflectionType *) * number_of_parameters;

    auto ob = NewObject<MIOReflectionFunction>(placement_size);
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
