#include "msg-garbage-collector.h"
#include "managed-allocator.h"
#include "vm-thread.h"
#include "vm-memory-segment.h"
#include "vm-object-scanner.h"
#include "vm-object-surface.h"
#include "vm-objects.h"

namespace mio {

MSGGarbageCollector::MSGGarbageCollector(ManagedAllocator *allocator,
                                         MemorySegment *root,
                                         Thread *main_thread,
                                         bool trace_logging)
    : trace_logging_(trace_logging)
    , root_(DCHECK_NOTNULL(root))
    , main_thread_(DCHECK_NOTNULL(main_thread))
    , current_thread_(main_thread)
    , handle_header_(static_cast<HeapObject *>(::malloc(HeapObject::kListEntryOffset)))
    , gray_header_(static_cast<HeapObject *>(::malloc(HeapObject::kListEntryOffset)))
    , gray_again_header_(static_cast<HeapObject *>(::malloc(HeapObject::kListEntryOffset)))
    , allocator_(DCHECK_NOTNULL(allocator)) {
    handle_header_->InitEntry();
    gray_header_->InitEntry();
    gray_again_header_->InitEntry();

    for (int i = 0; i < kMaxGeneration; ++i) {
        generations_[i] = static_cast<HeapObject *>(::malloc(HeapObject::kListEntryOffset));
        generations_[i]->InitEntry();
    }
}

/*virtual*/
MSGGarbageCollector::~MSGGarbageCollector() {
    for (int i = 0; i < kMaxGeneration; ++i) {
        ::free(generations_[i]);
    }
    ::free(gray_again_header_);
    ::free(gray_header_);
    ::free(handle_header_);
}

////////////////////////////////////////////////////////////////////////////////
/// Implements for ObjectFactory
////////////////////////////////////////////////////////////////////////////////

/*virtual*/
Handle<MIOString> MSGGarbageCollector::GetOrNewString(const mio_strbuf_t *bufs, int n) {
    auto payload_length = 0;
    DCHECK_GE(n, 0);
    for (int i = 0; i < n; ++i) {
        payload_length += bufs[i].n;
    }

    auto total_size = payload_length + 1 + MIOString::kDataOffset; // '\0' + MIOStringHeader
    auto ob = NewObject<MIOString>(total_size, 0);

    ob->SetLength(payload_length);
    auto p = ob->GetMutableData();
    for (int i = 0; i < n; ++i) {
        memcpy(p, bufs[i].z, bufs[i].n);
        p += bufs[i].n;
    }
    ob->GetMutableData()[payload_length] = '\0';

    if (ob->GetLength() > kMaxUniqueStringSize) {
        return make_handle(ob);
    }

    auto iter = unique_strings_.find(ob->GetData());
    if (iter != unique_strings_.end()) {
        HORemove(ob);
        allocator_->Free(ob);
        ob = const_cast<MIOString *>(MIOString::OffsetOfData(*iter));
    } else {
        unique_strings_.insert(ob->GetData());
    }

    return make_handle(ob);
}

/*virtual*/
Handle<MIOClosure>
MSGGarbageCollector::CreateClosure(Handle<MIOFunction> function,
                                   int up_values_size) {
    auto placement_size = static_cast<int>(MIOClosure::kUpValesOffset +
            up_values_size * sizeof(UpValDesc));
    auto ob = NewObject<MIOClosure>(placement_size, 0);

    ob->SetFlags(0);
    ob->SetFunction(function.get());
    ob->SetUpValueSize(up_values_size);
    return make_handle(ob);
}

/*virtual*/
Handle<MIONativeFunction>
MSGGarbageCollector::CreateNativeFunction(const char *signature,
                                          MIOFunctionPrototype pointer) {
    Handle<MIOString> sign(ObjectFactory::GetOrNewString(signature,
                                                         static_cast<int>(strlen(signature))));
    auto ob = NewObject<MIONativeFunction>(MIONativeFunction::kMIONativeFunctionOffset, 0);
    ob->SetSignature(sign.get());
    ob->SetNativePointer(pointer);
    return make_handle(ob);
}

/*virtual*/
Handle<MIONormalFunction>
MSGGarbageCollector::CreateNormalFunction(const std::vector<Handle<HeapObject>> &constant_objects,
                                          const void *constant_primitive_data,
                                          int constant_primitive_size,
                                          const void *code,
                                          int code_size) {
    DCHECK_EQ(0, code_size % sizeof(uint64_t));

    auto placement_size = static_cast<int>(MIONormalFunction::kHeaderOffset +
                                           constant_primitive_size +
                                           constant_objects.size() * kObjectReferenceSize + code_size);
    auto ob = NewObject<MIONormalFunction>(placement_size, 0);

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
Handle<MIOHashMap>
MSGGarbageCollector::CreateHashMap(int seed, int initial_slots,
                                   Handle<MIOReflectionType> key,
                                   Handle<MIOReflectionType> value) {
    DCHECK(key->CanBeKey());
    auto ob = NewObject<MIOHashMap>(MIOHashMap::kMIOHashMapOffset, 0);
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
MSGGarbageCollector::CreateError(const char *message, int position,
                                   Handle<MIOError> linked) {
    auto ob = NewObject<MIOError>(MIOError::kMIOErrorOffset, 0);
    ob->SetPosition(position);

    auto msg = ObjectFactory::GetOrNewString(message,
                                             static_cast<int>(strlen(message)));
    ob->SetMessage(msg.get());
    ob->SetLinkedError(linked.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOUnion>
MSGGarbageCollector::CreateUnion(const void *data, int size,
                                Handle<MIOReflectionType> type_info) {
    DCHECK_GE(size, 0);
    DCHECK_LE(size, kMaxReferenceValueSize);

    auto ob = NewObject<MIOUnion>(MIOUnion::kMIOUnionOffset, 0);
    ob->SetTypeInfo(type_info.get());
    if (size > 0) {
        memcpy(ob->GetMutableData(), data, size);
    }
    return make_handle(ob);
}

/*virtual*/
Handle<MIOUpValue>
MSGGarbageCollector::GetOrNewUpValue(const void *data, int size,
                                    int32_t unique_id, bool is_primitive) {
    auto iter = unique_upvals_.find(unique_id);
    if (iter != unique_upvals_.end()) {
        return make_handle(iter->second);
    }

    auto placement_size = MIOUpValue::kHeaderOffset + size;
    auto ob = NewObject<MIOUpValue>(placement_size, 0);

    ob->SetFlags((unique_id << 1) | (is_primitive ? 0x0 : 0x1));
    ob->SetValueSize(size);
    memcpy(ob->GetValue(), data, size);

    unique_upvals_.emplace(unique_id, ob);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionVoid>
MSGGarbageCollector::CreateReflectionVoid(int64_t tid) {
    auto ob = NewObject<MIOReflectionVoid>(MIOReflectionVoid::kMIOReflectionVoidOffset, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionIntegral>
MSGGarbageCollector::CreateReflectionIntegral(int64_t tid, int bitwide) {
    auto ob = NewObject<MIOReflectionIntegral>(MIOReflectionIntegral::kMIOReflectionIntegralOffset, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize((bitwide + 7) / 8);
    ob->SetBitWide(bitwide);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionFloating>
MSGGarbageCollector::CreateReflectionFloating(int64_t tid, int bitwide) {
    auto ob = NewObject<MIOReflectionFloating>(MIOReflectionFloating::kMIOReflectionFloatingOffset, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize((bitwide + 7) / 8);
    ob->SetBitWide(bitwide);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionString>
MSGGarbageCollector::CreateReflectionString(int64_t tid) {
    auto ob = NewObject<MIOReflectionString>(MIOReflectionString::kMIOReflectionStringOffset, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionError>
MSGGarbageCollector::CreateReflectionError(int64_t tid) {
    auto ob = NewObject<MIOReflectionError>(MIOReflectionError::kMIOReflectionErrorOffset, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionUnion>
MSGGarbageCollector::CreateReflectionUnion(int64_t tid) {
    auto ob = NewObject<MIOReflectionUnion>(MIOReflectionUnion::kMIOReflectionUnionOffset, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionArray>
MSGGarbageCollector::CreateReflectionArray(int64_t tid,
                      Handle<MIOReflectionType> element) {
    auto ob = NewObject<MIOReflectionArray>(MIOReflectionArray::kMIOReflectionArrayOffset, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetElement(element.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionSlice>
MSGGarbageCollector::CreateReflectionSlice(int64_t tid,
                      Handle<MIOReflectionType> element) {
    auto ob = NewObject<MIOReflectionSlice>(MIOReflectionSlice::kMIOReflectionSliceOffset, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetElement(element.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionMap>
MSGGarbageCollector::CreateReflectionMap(int64_t tid,
                                           Handle<MIOReflectionType> key,
                                           Handle<MIOReflectionType> value) {
    auto ob = NewObject<MIOReflectionMap>(MIOReflectionMap::kMIOReflectionMapOffset, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetKey(key.get());
    ob->SetValue(value.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionFunction>
MSGGarbageCollector::CreateReflectionFunction(int64_t tid, Handle<MIOReflectionType> return_type,
                                                int number_of_parameters,
                                                const std::vector<Handle<MIOReflectionType>> &parameters) {
    int placement_size = MIOReflectionFunction::kParamtersOffset +
    sizeof(MIOReflectionType *) * number_of_parameters;
    
    auto ob = NewObject<MIOReflectionFunction>(placement_size, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetNumberOfParameters(number_of_parameters);
    ob->SetReturn(return_type.get());
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        ob->GetParamters()[i] = parameters[i].get();
    }
    return make_handle(ob);
}

/*virtual*/
void MSGGarbageCollector::Step(int tick) {
    if (pause_) {
        return; // if pause, then ignore.
    }

    switch (phase_) {
        case kPause:
            MarkRoot();
            start_tick_ = tick;
            memset(sweep_info_, 0, arraysize(sweep_info_) * sizeof(sweep_info_[0]));
            break;

        case kPropagate:
            if (HOIsNotEmpty(gray_header_)) {
                Propagate();
            } else {
                Atomic();
            }
            break;

        case kSweepYoung:
            SweepYoung();
            break;

        case kSweepOld:
            SweepOld();
            break;

        case kFinialize:
            phase_ = kPause;
            if (trace_logging_) {
                DLOG(INFO) << "gc finialize, total tick: " << tick - start_tick_;
            }
            start_tick_ = 0;
            break;

        default:
            break;
    }
    tick_ = tick;
}

/*virtual*/
void MSGGarbageCollector::WriteBarrier(HeapObject *target, HeapObject *other) {
    if (target->GetGeneration() > other->GetGeneration()) {
        other->SetGeneration(target->GetGeneration());
        if (other->GetColor() != kGray) {
            HORemove(other);
            HOInsertHead(generations_[target->GetGeneration()], other);
        }
    }

    if (other->GetGeneration() > target->GetGeneration()) {
        target->SetGeneration(other->GetGeneration());
        if (target->GetColor() != kGray) {
            HORemove(target);
            HOInsertHead(generations_[other->GetGeneration()], target);
        }
    }
}

/*virtual*/
void MSGGarbageCollector::FullGC() {
    while (phase_ != kPause) {
        Step(0);
    }

    need_full_gc_ = true;
    Step(0);
    while (phase_ != kPause) {
        Step(0);
    }
    need_full_gc_ = false;
}

void MSGGarbageCollector::MarkRoot() {
    ObjectScanner scanner;

    auto buf = root_->buf<HeapObject *>();
    for (int i = 0; i < buf.n; ++i) {
        MarkGray(buf.z[i]);
    }

    buf = current_thread_->o_stack()->buf<HeapObject *>();
    for (int i = 0; i < buf.n; ++i) {
        MarkGray(buf.z[i]);
    }

    std::vector<MIOFunction *> call_stack;
    current_thread_->GetCallStack(&call_stack);
    for (int i = 0; i < call_stack.size(); ++i) {
        MarkGray(call_stack[i]);
    }

    while (HOIsNotEmpty(handle_header_)) {
        auto x = handle_header_->GetNext();
        HORemove(x);
        if (x->IsGrabbed()) {
            x->SetColor(kGray);
            HOInsertHead(gray_header_, x);
        } else {
            HOInsertHead(generations_[x->GetGeneration()], x);
        }
    }

    phase_ = kPropagate;
}

void MSGGarbageCollector::Propagate() {
    ObjectScanner scanner;
    auto n = 0;

    while (n < propagate_speed_ && HOIsNotEmpty(gray_header_)) {
        auto x = gray_header_->GetNext();
        scanner.Scan(x, [this, &n] (HeapObject *ob) {
            switch (ob->GetColor()) {
                case kWhite0:
                case kWhite1:
                    ob->SetColor(kBlack);
                    break;

                case kGray:
                    Gray2Black(ob);
                    break;

                case kBlack:
                    break;
            }
            HORemove(ob);
            DCHECK_NE(ob, gray_again_header_);
            HOInsertHead(gray_again_header_, ob);

            n++;
        });
    }

    if (trace_logging_) {
        DLOG(INFO) << "propagate: " << n << " objects.";
    }
}

void MSGGarbageCollector::Atomic() {
    DCHECK(HOIsEmpty(gray_header_));
    while (HOIsNotEmpty(gray_again_header_)) {
        auto x = gray_again_header_->GetNext();
        HORemove(x);
        HOInsertHead(gray_header_, x);
    }

    MarkRoot();
    while (HOIsNotEmpty(gray_header_)) {
        Propagate();
    }

    while (HOIsNotEmpty(gray_again_header_)) {
        auto x = gray_again_header_->GetNext();
        HORemove(x);
        HOInsertHead(generations_[x->GetGeneration()], x);
    }
    SwitchWhite();
    phase_ = kSweepYoung;
    sweep_info_[0].iter = generations_[0]->GetNext();
}

void MSGGarbageCollector::SweepYoung() {
    auto header = generations_[0];
    auto n = 0;

    auto info = &sweep_info_[0];
    ++info->times;
    while (n < sweep_speed_ && info->iter != header) {
        auto x = info->iter; info->iter = info->iter->GetNext();
        if (x->IsGrabbed()) {
            ++info->grabbed;
            x->SetColor(white_);
            HORemove(x);
            HOInsertHead(handle_header_, x);
        } else if (x->GetColor() == PrevWhite()) {
            ++info->release;
            info->release_bytes += x->GetSize();
            HORemove(x);
            DeleteObject(x);
        } else if (x->GetColor() == white_) {
            // junks
            ++info->junks;
            info->junks_bytes += x->GetSize();
        } else {
            ++info->grow_up;
            x->SetGeneration(1);
            x->SetColor(white_);
            HORemove(x);
            HOInsertHead(generations_[1], x); // move to old generation.
        }
        n++;
    }
    if (info->iter == header) {
        if (need_full_gc_) {
            phase_ = kSweepOld;
            sweep_info_[1].iter = generations_[1]->GetNext();
        } else {
            phase_ = kFinialize;
        }

        if (trace_logging_) {
            DLOG(INFO) << "------[young generation]------";
            DLOG(INFO) << "-- release: " << info->release << ", " << info->release_bytes;
            DLOG(INFO) << "-- junks: "   << info->junks   << ", " << info->junks_bytes;
            DLOG(INFO) << "-- grabbed: " << info->grabbed;
            DLOG(INFO) << "-- grow up: " << info->grow_up;
        }
    }
}

void MSGGarbageCollector::SweepOld() {
    auto header = generations_[1];
    auto n = 0;

    auto info = &sweep_info_[1];
    ++info->times;
    while (n < sweep_speed_ && info->iter != header) {
        auto x = info->iter; info->iter = info->iter->GetNext();

        if (x->IsGrabbed()) {
            ++info->grabbed;
            x->SetColor(white_);
            HORemove(x);
            HOInsertHead(handle_header_, x);
        } else if (x->GetColor() == PrevWhite()) {
            ++info->release;
            info->release_bytes += x->GetSize();
            HORemove(x);
            DeleteObject(x);
        } else if (x->GetColor() == white_) {
            // junks
            ++info->junks;
            info->junks_bytes += x->GetSize();
        }
        n++;
    }
    if (info->iter == header) {
        phase_ = kFinialize;

        if (trace_logging_) {
            DLOG(INFO) << "------[old generation]------";
            DLOG(INFO) << "-- release: " << info->release << ", " << info->release_bytes;
            DLOG(INFO) << "-- junks: "   << info->junks   << ", " << info->junks_bytes;
            DLOG(INFO) << "-- grabbed: " << info->grabbed;
        }
    }
}

void MSGGarbageCollector::DeleteObject(const HeapObject *ob) {
    switch (DCHECK_NOTNULL(ob)->GetKind()) {
        case HeapObject::kHashMap: {
            auto map = const_cast<HeapObject *>(ob)->AsHashMap();
            MIOHashMapSurface surface(map, allocator_);
            surface.CleanAll();
            allocator_->Free(map->GetSlots());
        } break;

        case HeapObject::kString: {
            auto str = ob->AsString();
            if (str->GetLength() <= kMaxUniqueStringSize) {
                unique_strings_.erase(str->GetData());
            }
        } break;

        case HeapObject::kUpValue: {
            auto val = ob->AsUpValue();
            unique_upvals_.erase(val->GetUniqueId());
        } break;

        case HeapObject::kNormalFunction: {
            auto fn = ob->AsNormalFunction();
            allocator_->Free(fn->GetDebugInfo());
        } break;

        default:
            break;
    }
    Round32BytesFill(kFreeMemoryBytes, const_cast<HeapObject *>(ob), ob->GetSize());
    allocator_->Free(ob);
}

} // namespace mio
