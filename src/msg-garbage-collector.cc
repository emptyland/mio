#include "msg-garbage-collector.h"
#include "managed-allocator.h"
#include "vm-thread.h"
#include "vm-memory-segment.h"
#include "vm-object-scanner.h"
#include "vm-objects.h"


namespace mio {

MSGGarbageCollector::MSGGarbageCollector(MemorySegment *root, Thread *main_thread)
    : root_(DCHECK_NOTNULL(root))
    , main_thread_(DCHECK_NOTNULL(main_thread))
    , current_thread_(main_thread)
    , handle_header_(static_cast<HeapObject *>(::malloc(HeapObject::kListEntryOffset)))
    , gray_header_(static_cast<HeapObject *>(::malloc(HeapObject::kListEntryOffset)))
    , gray_again_header_(static_cast<HeapObject *>(::malloc(HeapObject::kListEntryOffset)))
    , allocator_(new ManagedAllocator()) {
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
Handle<MIOString>
MSGGarbageCollector::GetOrNewString(const char *z, int n) {
    return ObjectFactory::CreateString(z, n);
}

/*virtual*/
Handle<MIOString> MSGGarbageCollector::CreateString(const mio_strbuf_t *bufs, int n) {
    auto payload_length = 0;
    DCHECK_GE(n, 0);
    for (int i = 0; i < n; ++i) {
        payload_length += bufs[i].n;
    }

    auto total_size = payload_length + 1 + MIOString::kDataOffset; // '\0' + MIOStringHeader
    auto ob = NewObject<MIOString>(total_size, 0);

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
    Handle<MIOString> sign(ObjectFactory::CreateString(signature,
                                                       static_cast<int>(strlen(signature))));
    auto ob = NewObject<MIONativeFunction>(MIONativeFunction::kMIONativeFunctionOffset, 0);
    ob->SetSignature(sign.get());
    ob->SetNativePointer(pointer);
    printf("nfn %p %p %s\n", ob, sign.get(), ob->GetSignature()->GetData());
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
Handle<MIOHashMap> MSGGarbageCollector::CreateHashMap(int seed, uint32_t flags) {
    // TODO:
    return Handle<MIOHashMap>();
}

/*virtual*/
Handle<MIOError>
MSGGarbageCollector::CreateError(const char *message, int position,
                                   Handle<MIOError> linked) {
    auto ob = NewObject<MIOError>(MIOError::kMIOErrorOffset, 0);
    ob->SetPosition(position);
    ob->SetMessage(GetOrNewString(message, static_cast<int>(strlen(message))).get());
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
        memcpy(ob->mutable_data(), data, size);
    }
    return make_handle(ob);
}

/*virtual*/
Handle<MIOUpValue>
MSGGarbageCollector::GetOrNewUpValue(const void *data, int size,
                                    int32_t unique_id, bool is_primitive) {
    auto placement_size = MIOUpValue::kHeaderOffset + size;
    auto ob = NewObject<MIOUpValue>(placement_size, 0);

    ob->SetFlags((unique_id << 1) | (is_primitive ? 0x0 : 0x1));
    ob->SetValueSize(size);
    memcpy(ob->GetValue(), data, size);
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
            break;

        case kRemark:
            Remark();
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
            break;

        case kFinialize:
            phase_ = kPause;
            DLOG(INFO) << "gc finialize, total tick: " << tick - start_tick_;
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
    while (phase_ != kFinialize) {
        Step(0);
    }
}

void MSGGarbageCollector::MarkRoot() {
    ObjectScanner scanner;

    auto buf = root_->buf<HeapObject *>();
    for (int i = 0; i < buf.n; ++i) {
        RecursiveMarkGray(&scanner, buf.z[i]);
    }

    buf = current_thread_->o_stack()->buf<HeapObject *>();
    for (int i = 0; i < buf.n; ++i) {
        RecursiveMarkGray(&scanner, buf.z[i]);
    }

    std::vector<MIOFunction *> call_stack;
    current_thread_->GetCallStack(&call_stack);
    for (int i = 0; i < call_stack.size(); ++i) {
        RecursiveMarkGray(&scanner, call_stack[i]);
    }

    phase_ = kRemark;
}

void MSGGarbageCollector::Remark() {
    ObjectScanner scanner;

    auto x = handle_header_->GetNext();
    while (x != handle_header_) {
        auto next = x->GetNext();
        if (x->IsHandle()) {
            scanner.Scan(x, [&] (HeapObject *ob) {
                x->SetColor(kGray);
                HORemove(x);
                HOInsertHead(gray_header_, x);
            });
        } else {
            // put down x to generation list.
            x->SetColor(white_);
            HORemove(x);
            HOInsertHead(generations_[x->GetGeneration()], x);
        }
        x = next;
    }
}

void MSGGarbageCollector::Propagate() {
    ObjectScanner scanner;
    auto remain = 0;

    while (remain < propagate_speed_ && HOIsNotEmpty(gray_header_)) {
        auto x = gray_header_->GetNext();
        auto next = x->GetNext();

        scanner.Scan(x, [this, &remain] (HeapObject *ob) {
            if (ob->GetColor() == kGray) {
                Gray2Black(ob);
            }
            HORemove(ob);
            HOInsertHead(gray_again_header_, ob);

            remain++;
        });
        gray_header_->SetNext(next);
    }

    DLOG(INFO) << "propagate: " << remain << " objects.";
}

void MSGGarbageCollector::Atomic() {
    DCHECK(HOIsEmpty(gray_header_));
    std::swap(gray_again_header_, gray_header_);

    MarkRoot();
    Remark();
    while (HOIsNotEmpty(gray_header_)) {
        Propagate();
    }

    SwitchWhite();
    phase_ = kSweepYoung;
}

void MSGGarbageCollector::SweepYoung() {
    auto header = generations_[0];
    auto n = 0;
    while (n < sweep_speed_ && HOIsNotEmpty(header)) {
        auto x = header->GetNext();
        auto next = x->GetNext();

        if (x->GetColor() == PrevWhite()) {
            HORemove(x);
            DeleteObject(x);
        } else if (x->GetColor() == kBlack) {
            HORemove(x);
            HOInsertHead(generations_[1], x); // move to old generation.
        }

        x = next;
    }
    if (HOIsEmpty(header)) {
        phase_ = kFinialize;
    }
}

void MSGGarbageCollector::RecursiveMarkGray(ObjectScanner *scanner, HeapObject *x) {
    if (!x || x->GetColor() == kGray) {
        return;
    }
    scanner->Scan(x, [this] (HeapObject *ob) {
        if (ob->GetColor() == white_) {
            this->White2Gray(ob);
        }
        if (ob->IsHandle()) {
            HORemove(ob);
            HOInsertHead(handle_header_, ob);
        } else {
            HORemove(ob);
            HOInsertHead(gray_header_, ob);
        }
    });
}

} // namespace mio
