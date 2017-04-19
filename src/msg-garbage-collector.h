#ifndef MSG_GARBAGE_COLLECTOR_H_
#define MSG_GARBAGE_COLLECTOR_H_

#include "vm-garbage-collector.h"
#include "vm-objects.h"
#include "managed-allocator.h"
#include "base.h"
#include "glog/logging.h"

namespace mio {

class MemorySegment;
class Thread;
class ObjectScanner;
class ManagedAllocator;

/**
 * The Mark-Sweep-Generation GC
 */
class MSGGarbageCollector : public GarbageCollector {
public:
    enum Phase: int {
        kPause,
        kRemark,
        kPropagate,
        kSweepYoung,
        kSweepOld,
        kFinialize,
    };

    enum Color: int {
        kWhite0,
        kWhite1,
        kGray,
        kBlack,
    };

    static const int kMaxGeneration = 2;
    static const int kDefaultPropagateSpeed = 5;
    static const int kDefaultSweepSpeed = 10;

    MSGGarbageCollector(MemorySegment *root, Thread *main_thread);
    virtual ~MSGGarbageCollector();

    ////////////////////////////////////////////////////////////////////////////
    /// Implements for ObjectFactory
    ////////////////////////////////////////////////////////////////////////////

    virtual Handle<MIOString> CreateString(const mio_strbuf_t *bufs, int n) override;

    virtual Handle<MIOString>
    GetOrNewString(const char *z, int n) override;

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
                         int code_size) override;

    virtual Handle<MIOHashMap>
    CreateHashMap(int seed, uint32_t flags) override;

    virtual
    Handle<MIOError> CreateError(const char *message, int position,
                                 Handle<MIOError> linked) override;

    virtual
    Handle<MIOUnion> CreateUnion(const void *data, int size,
                                 Handle<MIOReflectionType> type_info) override;

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

    virtual Handle<MIOReflectionString>
    CreateReflectionString(int64_t tid) override;

    virtual Handle<MIOReflectionError>
    CreateReflectionError(int64_t tid) override;

    virtual Handle<MIOReflectionUnion>
    CreateReflectionUnion(int64_t tid) override;

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
    virtual void Active(bool pause) override { pause_ = pause; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MSGGarbageCollector)
private:
    void SwitchWhite() { white_ = PrevWhite(); }

    Color PrevWhite() { return (white_ == kWhite0) ? kWhite1 : kWhite0; }

    void MarkRoot();
    void Remark();
    void Propagate();
    void Atomic();
    void SweepYoung();

    void RecursiveMarkGray(ObjectScanner *scanner, HeapObject *x);

    template<class T>
    inline T *NewObject(int placement_size, int g);

    void DeleteObject(const HeapObject *ob) { allocator_->DeleteObject(ob); }

    void White2Gray(HeapObject *ob) {
        DCHECK_EQ(ob->GetColor(), white_);
        ob->SetColor(kGray);
    }

    void Gray2Black(HeapObject *ob) {
        DCHECK_EQ(kGray, ob->GetColor());
        ob->SetColor(kGray);
    }

    bool pause_ = false;
    Color white_ = kWhite0;
    Phase phase_ = kPause;
    int tick_ = 0;
    int start_tick_ = 0;
    int propagate_speed_ = kDefaultPropagateSpeed;
    int sweep_speed_ = kDefaultSweepSpeed;

    MemorySegment *root_;

    Thread *main_thread_;
    Thread *current_thread_;

    HeapObject  *handle_header_;
    HeapObject  *gray_header_;
    HeapObject  *gray_again_header_;
    HeapObject  *generations_[kMaxGeneration];
    ManagedAllocator *allocator_;
}; // class MSGGarbageCollector

template<class T>
inline T *MSGGarbageCollector::NewObject(int placement_size, int g) {
    auto ob = allocator_->NewObject<T>(placement_size);
    HOInsertHead(generations_[g], ob);
    ob->SetColor(white_);
    return ob;
}

} // namespace mio

#endif // MSG_GARBAGE_COLLECTOR_H_
