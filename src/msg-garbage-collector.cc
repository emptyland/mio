#include "msg-garbage-collector.h"
#include "managed-allocator.h"
#include "vm-code-cache.h"
#include "vm-thread.h"
#include "vm-memory-segment.h"
#include "vm-object-scanner.h"
#include "vm-object-surface.h"
#include "vm-objects.h"

namespace mio {

#define NEW_FIXED_SIZE_OBJECT(var, name, g) \
    NEW_OBJECT(var, name, (name::k##name##Offset), (g))

#define NEW_OBJECT(var, name, size, g) \
    auto var = NewObject<name>((size), (g)); \
    if (!(var)) { \
        return Handle<name>(); \
    } (void)0

bool ShouldProcessWeakMap(HeapObject *x) {
    if (!x->IsHashMap()) {
        return false;
    }

    auto map = x->AsHashMap();
    if (map->GetKey()->IsObject() &&
        (map->GetWeakFlags() & MIOHashMap::kWeakKeyFlag)) {
        return true;
    }
    if (map->GetValue()->IsObject() &&
        (map->GetWeakFlags() & MIOHashMap::kWeakValueFlag)) {
        return true;
    }
    
    return false;
}

MSGGarbageCollector::MSGGarbageCollector(ManagedAllocator *allocator,
                                         CodeCache *code_cache,
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
    , weak_header_(static_cast<HeapObject *>(::malloc(HeapObject::kListEntryOffset)))
    , allocator_(DCHECK_NOTNULL(allocator))
    , code_cache_(DCHECK_NOTNULL(code_cache)) {
    handle_header_->InitEntry();
    gray_header_->InitEntry();
    gray_again_header_->InitEntry();
    weak_header_->InitEntry();

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
    NEW_OBJECT(ob, MIOString, total_size, 0);

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
    NEW_OBJECT(ob, MIOClosure, placement_size, 0);

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
    NEW_FIXED_SIZE_OBJECT(ob, MIONativeFunction, 0);
    ob->SetSignature(sign.get());
    ob->SetNativePointer(pointer);
    ob->SetNativeWarperIndex(nullptr);
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
    NEW_OBJECT(ob, MIONormalFunction, placement_size, 0);

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
MSGGarbageCollector::CreateVector(int initial_size,
                                  Handle<MIOReflectionType> element) {
    DCHECK_GE(initial_size, 0);
    DCHECK_NE(HeapObject::kReflectionVoid, element->GetKind());

    NEW_FIXED_SIZE_OBJECT(ob, MIOVector, 0);
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
Handle<MIOSlice> MSGGarbageCollector::CreateSlice(int begin, int size,
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
    NEW_FIXED_SIZE_OBJECT(ob, MIOSlice, 0);
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
MSGGarbageCollector::CreateHashMap(int seed, int initial_slots,
                                   Handle<MIOReflectionType> key,
                                   Handle<MIOReflectionType> value) {
    DCHECK(key->CanBeKey());
    NEW_FIXED_SIZE_OBJECT(ob, MIOHashMap, 0);
    ob->SetSeed(seed);
    ob->SetKey(key.get());
    ob->SetValue(value.get());
    ob->SetSize(0);

    DCHECK_GE(initial_slots, 0);
    ob->SetSlotSize(initial_slots);
    ob->SetSlots(nullptr);
    if (ob->GetSlotSize() > 0) {
        auto slots_placement_size = static_cast<int>(sizeof(MIOHashMap::Slot) * ob->GetSlotSize());
        auto slots = static_cast<MIOHashMap::Slot *>(allocator_->Allocate(slots_placement_size));
        if (!slots) {
            return Handle<MIOHashMap>();
        }
        memset(slots, 0, slots_placement_size);
        ob->SetSlots(slots);
    }

    return make_handle(ob);
}

/*virtual*/
Handle<MIOError>
MSGGarbageCollector::CreateError(Handle<MIOString> msg,
                                 Handle<MIOString> file_name, int position,
                                 Handle<MIOError> linked) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOError, 0);
    ob->SetFileName(file_name.get());
    ob->SetPosition(position);
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

    NEW_FIXED_SIZE_OBJECT(ob, MIOUnion, 0);
    ob->SetTypeInfo(type_info.get());
    if (size > 0) {
        memcpy(ob->GetMutableData(), data, size);
    }
    return make_handle(ob);
}

/*virtual*/
Handle<MIOExternal> MSGGarbageCollector::CreateExternal(intptr_t type_code,
                                                        void *value) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOExternal, 0);
    ob->SetTypeCode(type_code);
    ob->SetValue(value);
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
    NEW_OBJECT(ob, MIOUpValue, placement_size, 0);

    ob->SetFlags((unique_id << 1) | (is_primitive ? 0x0 : 0x1));
    ob->SetValueSize(size);
    memcpy(ob->GetValue(), data, size);

    unique_upvals_.emplace(unique_id, ob);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionVoid>
MSGGarbageCollector::CreateReflectionVoid(int64_t tid) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionVoid, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionIntegral>
MSGGarbageCollector::CreateReflectionIntegral(int64_t tid, int bitwide) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionIntegral, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize((bitwide + 7) / 8);
    ob->SetBitWide(bitwide);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionFloating>
MSGGarbageCollector::CreateReflectionFloating(int64_t tid, int bitwide) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionFloating, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize((bitwide + 7) / 8);
    ob->SetBitWide(bitwide);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionString>
MSGGarbageCollector::CreateReflectionString(int64_t tid) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionString, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionError>
MSGGarbageCollector::CreateReflectionError(int64_t tid) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionError, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionUnion>
MSGGarbageCollector::CreateReflectionUnion(int64_t tid) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionUnion, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionExternal>
MSGGarbageCollector::CreateReflectionExternal(int64_t tid) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionExternal, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionArray>
MSGGarbageCollector::CreateReflectionArray(int64_t tid,
                      Handle<MIOReflectionType> element) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionArray, 0);
    ob->SetTid(tid);
    ob->SetReferencedSize(kObjectReferenceSize);
    ob->SetElement(element.get());
    return make_handle(ob);
}

/*virtual*/
Handle<MIOReflectionSlice>
MSGGarbageCollector::CreateReflectionSlice(int64_t tid,
                      Handle<MIOReflectionType> element) {
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionSlice, 0);
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
    NEW_FIXED_SIZE_OBJECT(ob, MIOReflectionMap, 0);
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
    
    NEW_OBJECT(ob, MIOReflectionFunction, placement_size, 0);
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

    if (target->GetColor() == kBlack) {
        other->SetColor(kBlack);
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
        if (ShouldProcessWeakMap(x)) {
            x->SetColor(kBlack);

            HORemove(x);
            DCHECK_NE(x, gray_again_header_);
            HOInsertHead(weak_header_, x);
            n++;
            continue;
        }

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

    while (HOIsNotEmpty(weak_header_)) {
        CollectWeakReferences();
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

void MSGGarbageCollector::CollectWeakReferences() {
    auto n = 0;
    auto info = &sweep_info_[kWeakReferenceSweep];

    ++info->times;
    while (n < sweep_speed_ && HOIsNotEmpty(weak_header_)) {
        auto x = weak_header_->GetNext();
        auto map = DCHECK_NOTNULL(x->AsHashMap());
        for (int i = 0; i < map->GetSlotSize(); ++i) {
            auto prev = reinterpret_cast<MIOPair *>(map->GetSlot(i));
            auto node = prev->GetNext();
            while (node) {
                bool should_sweep = false;
                if ((map->GetWeakFlags() & MIOHashMap::kWeakKeyFlag) &&
                    map->GetKey()->IsObject()) {
                    auto key = *static_cast<HeapObject **>(node->GetKey());
                    if (key->GetColor() == PrevWhite()) {
                        should_sweep = true;
                    }
                }

                if (map->GetWeakFlags() & MIOHashMap::kWeakValueFlag &&
                    map->GetValue()->IsObject()) {
                    auto value = *static_cast<HeapObject **>(node->GetValue());
                    if (value->GetColor() == kWhite0 ||
                        value->GetColor() == kWhite1) {
                        should_sweep = true;
                    }
                }

                if (should_sweep) {
                    prev->SetNext(node->GetNext());
                    allocator_->Free(node);
                    node = prev->GetNext();
                } else {
                    prev = node;
                    node = node->GetNext();
                }
            }
        }
        HORemove(x);
        HOInsertHead(generations_[x->GetGeneration()], x); // move to old generation.
        n++;
    }
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
        } else {
            x->SetColor(white_);
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

        case HeapObject::kNativeFunction: {
            auto fn = ob->AsNativeFunction();
            code_cache_->Free(CodeRef(fn->GetNativeWarperIndex()));
        } break;

        default:
            break;
    }
    //printf("!!!delete ob: %p, kind: %d\n", ob, ob->GetKind());
    Round32BytesFill(kFreeMemoryBytes, const_cast<HeapObject *>(ob), ob->GetSize());
    allocator_->Free(ob);
}

} // namespace mio
