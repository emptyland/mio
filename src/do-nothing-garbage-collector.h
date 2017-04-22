#ifndef MIO_DO_NOTHING_GARBAGE_COLLECTOR_H_
#define MIO_DO_NOTHING_GARBAGE_COLLECTOR_H_

#include "managed-allocator.h"
#include "vm-garbage-collector.h"
#include <vector>

namespace mio {

class HeapObject;

class DoNothingGarbageCollector : public GarbageCollector {
public:
    DoNothingGarbageCollector();
    virtual ~DoNothingGarbageCollector() override;

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
    CreateHashMap(int seed, int initial_slots, Handle<MIOReflectionType> key,
                  Handle<MIOReflectionType> value) override;

    virtual
    MIOHashMapSurface *MakeHashMapSurface(Handle<MIOHashMap> core) override;

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

    virtual void Step(int tick) override { /*DO-NOTHING*/ }
    virtual void WriteBarrier(HeapObject *target, HeapObject *other) override { /*DO-NOTHING*/ }
    virtual void FullGC() override { /*DO-NOTHING*/ }
    virtual void Active(bool pause) override { /*DO-NOTHING*/ }

    DISALLOW_IMPLICIT_CONSTRUCTORS(DoNothingGarbageCollector)
private:
    template<class T>
    inline T *NewObject(int placement_size) {
        auto ob = static_cast<T*>(allocator_->Allocate(placement_size));
        ob->Init(static_cast<HeapObject::Kind>(T::kSelfKind));
        objects_.push_back(ob);
        return ob;
    }

    std::unordered_map<int32_t, MIOUpValue *> upvalues_;
    std::vector<HeapObject *> objects_;
    ManagedAllocator *allocator_;
}; // class

} // namespace mio

#endif // MIO_DO_NOTHING_GARBAGE_COLLECTOR_H_
