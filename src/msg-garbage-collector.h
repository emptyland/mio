#ifndef MSG_GARBAGE_COLLECTOR_H_
#define MSG_GARBAGE_COLLECTOR_H_

#include "vm-garbage-collector.h"
#include "vm-objects.h"
#include "managed-allocator.h"
#include "base.h"
#include "glog/logging.h"
#include <unordered_set>

namespace mio {

class MemorySegment;
class Thread;
class ObjectScanner;
class CodeCache;
class ManagedAllocator;

struct SweepInfo {
    int times         = 0;
    int generation    = 0;
    int release       = 0;
    int release_bytes = 0;
    int grow_up       = 0;
    int junks         = 0;
    int junks_bytes   = 0;
    int grabbed       = 0;
    HeapObject *iter  = nullptr;

    SweepInfo() = default;
};

/**
 * The Mark-Sweep-Generation GC
 */
class MSGGarbageCollector : public GarbageCollector {
public:
    enum Phase: int {
        kPause,      // gc not running.
        kRemark,     // remark handled objects.
        kPropagate,  // mark gray objects to black.
        kSweepWeak,  // sweep weak references.
        kSweepYoung, // sweep young generation objects.
        kSweepOld,   // sweep old generation objects.
        kFinialize,  // gc loop finish.
    };

    enum Color: int {
        kWhite0,
        kWhite1,
        kGray,
        kBlack,
    };

    static const int kMaxGeneration = 2;
    static const int kWeakReferenceSweep = kMaxGeneration;
    static const int kDefaultPropagateSpeed = 50;
    static const int kDefaultSweepSpeed = 50;
    static const uint32_t kFreeMemoryBytes = 0xfeedfeed;

    MSGGarbageCollector(ManagedAllocator *allocator, CodeCache *code_cache,
                        MemorySegment *root, Thread *main_thread,
                        bool trace_logging);
    virtual ~MSGGarbageCollector();

    ////////////////////////////////////////////////////////////////////////////
    /// Implements for ObjectFactory
    ////////////////////////////////////////////////////////////////////////////
    virtual ManagedAllocator *allocator() override;

    virtual
    Handle<MIOString> GetOrNewString(const mio_strbuf_t *bufs, int n) override;

    virtual Handle<MIOClosure>
    CreateClosure(Handle<MIOFunction> function, int up_values_size) override;

    virtual Handle<MIONativeFunction>
    CreateNativeFunction(const char *signature,
                         MIOFunctionPrototype pointer) override;

    virtual Handle<MIONormalFunction>
    CreateNormalFunction(const std::vector<Handle<HeapObject>> &constant_objects,
                         const void *constant_primitive,
                         int constant_primitive_size,
                         const void *code,
                         int code_size,
                         int id) override;

    virtual Handle<MIOVector>
    CreateVector(int initial_size, Handle<MIOReflectionType> element) override;

    virtual Handle<MIOSlice> CreateSlice(int begin, int size,
                                         Handle<HeapObject> core) override;

    virtual Handle<MIOHashMap>
    CreateHashMap(int seed, int initial_slots, Handle<MIOReflectionType> key,
                  Handle<MIOReflectionType> value) override;

    virtual
    Handle<MIOError>
    CreateError(Handle<MIOString> msg, Handle<MIOString> file_name, int position,
                Handle<MIOError> linked) override;

    virtual
    Handle<MIOUnion> CreateUnion(const void *data, int size,
                                 Handle<MIOReflectionType> type_info) override;

    virtual
    Handle<MIOExternal> CreateExternal(intptr_t type_code, void *value) override;

    virtual
    Handle<MIOUpValue>
    GetOrNewUpValue(const void *data, int size, int32_t unique_id,
                    bool is_primitive) override;

    virtual
    Handle<MIOReflectionVoid> CreateReflectionVoid(int64_t tid) override;

    virtual
    Handle<MIOReflectionIntegral>
    CreateReflectionIntegral(int64_t tid, int bitwide) override;

    virtual
    Handle<MIOReflectionFloating>
    CreateReflectionFloating(int64_t tid, int bitwide) override;

    virtual Handle<MIOReflectionRef> CreateReflectionRef(int64_t tid) override;

    virtual Handle<MIOReflectionString>
    CreateReflectionString(int64_t tid) override;

    virtual Handle<MIOReflectionError>
    CreateReflectionError(int64_t tid) override;

    virtual Handle<MIOReflectionUnion>
    CreateReflectionUnion(int64_t tid) override;

    virtual Handle<MIOReflectionExternal>
    CreateReflectionExternal(int64_t tid) override;

    virtual
    Handle<MIOReflectionArray>
    CreateReflectionArray(int64_t tid,
                          Handle<MIOReflectionType> element) override;

    virtual
    Handle<MIOReflectionSlice>
    CreateReflectionSlice(int64_t tid,
                          Handle<MIOReflectionType> element) override;

    virtual
    Handle<MIOReflectionMap>
    CreateReflectionMap(int64_t tid,
                        Handle<MIOReflectionType> key,
                        Handle<MIOReflectionType> value) override;

    virtual
    Handle<MIOReflectionFunction>
    CreateReflectionFunction(int64_t tid, Handle<MIOReflectionType> return_type,
                             int number_of_parameters,
                             const std::vector<Handle<MIOReflectionType>> &parameters) override;

    ////////////////////////////////////////////////////////////////////////////
    /// Implements for GarbageCollector
    ////////////////////////////////////////////////////////////////////////////

    virtual void Step(int tick) override;
    virtual void WriteBarrier(HeapObject *target, HeapObject *other) override;
    virtual void FullGC() override;
    virtual void Active(bool active) override { pause_ = !active; }

    typedef std::unordered_set<const char *,
                               MIOStringDataHash,
                               MIOStringDataEqualTo> UniqueStringSet;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MSGGarbageCollector)
private:
    void SwitchWhite() { white_ = PrevWhite(); }

    Color PrevWhite() { return (white_ == kWhite0) ? kWhite1 : kWhite0; }

    void MarkRoot();
    void Propagate();
    void Atomic();
    void CollectWeakReferences();
    void SweepYoung();
    void SweepOld();

    void MarkGray(HeapObject *x) {
        if (!x || x->GetColor() == kGray || x->GetColor() == kBlack) {
            return;
        }
        White2Gray(x);
        HORemove(x);
        HOInsertHead(gray_header_, x);
    }

    template<class T>
    inline T *NewObject(int placement_size, int g);

    void DeleteObject(const HeapObject *ob);

    void White2Gray(HeapObject *ob) {
        DCHECK(ob->GetColor() == kWhite0 || ob->GetColor() == kWhite1)
            << "color: " << ob->GetColor()
            << ", kind: " << ob->GetKind();
        ob->SetColor(kGray);
    }

    void Gray2Black(HeapObject *ob) {
        DCHECK_EQ(kGray, ob->GetColor());
        ob->SetColor(kBlack);
    }

    void Black2White(HeapObject *ob, Color white) {
        DCHECK_EQ(kBlack, ob->GetColor());
        ob->SetColor(white);
    }

    bool pause_ = false;
    bool trace_logging_;
    Color white_ = kWhite0;
    Phase phase_ = kPause;
    int tick_ = 0;
    int start_tick_ = 0;
    int propagate_speed_ = kDefaultPropagateSpeed;
    int sweep_speed_ = kDefaultSweepSpeed;
    bool need_full_gc_ = false;

    UniqueStringSet unique_strings_;
    std::unordered_map<int32_t, MIOUpValue *> unique_upvals_;

    MemorySegment *root_;

    Thread *main_thread_;
    Thread *current_thread_;

    HeapObject  *handle_header_;
    HeapObject  *gray_header_;
    HeapObject  *gray_again_header_;
    HeapObject  *weak_header_;
    HeapObject  *generations_[kMaxGeneration];
    ManagedAllocator *allocator_;
    CodeCache *code_cache_;
    SweepInfo sweep_info_[kMaxGeneration + 1];
}; // class MSGGarbageCollector

template<class T>
inline T *MSGGarbageCollector::NewObject(int placement_size, int g) {
    auto ob = static_cast<T *>(allocator_->Allocate(placement_size));
    if (!ob) {
        return nullptr;
    }
    ob->Init(static_cast<HeapObject::Kind>(T::kSelfKind));
    ob->SetColor(white_);
    HOInsertHead(generations_[g], ob);
    return ob;
}

} // namespace mio

#endif // MSG_GARBAGE_COLLECTOR_H_
