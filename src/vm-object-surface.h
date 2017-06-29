#ifndef MIO_VM_OBJECT_SURFACE_H_
#define MIO_VM_OBJECT_SURFACE_H_

#include "vm-objects.h"
#include "vm-runtime.h"
#include "object-traits.h"
#include "managed-allocator.h"
#include "glog/logging.h"

namespace mio {

template<class K, class V> class MIOHashMapStub;
template<class T> class MIOArrayStub;

class MIOHashMapSurface {
public:
    constexpr static const float kRehashTopFactor    = 1.3f;
    constexpr static const float kRehashBottomFactor = 0.3f;
    static const int kMinSloSize = 7;

    typedef int (*Hash)(const void *, int);
    typedef bool (*EqualTo)(mio_buf_t<const void>, mio_buf_t<const void>);

    MIOHashMapSurface(MIOHashMap *core, ManagedAllocator *allocator);

    DEF_GETTER(Handle<MIOHashMap>, core)

    int size() const { return core_->GetSize(); }

    bool RawPut(const void *key, const void *value, bool *ok) {
        bool insert = false;
        auto pair = GetOrInsertRoom(key, &insert);
        if (!pair) {
            *ok = false;
        } else {
            memcpy(pair->GetValue(), value, value_size_);
        }
        return insert;
    }

    void *RawGet(const void *key) {
        int slot = 0;
        auto pair = GetRoom(key, &slot);
        return pair ? pair->GetValue() : nullptr;
    }

    bool RawDelete(const void *key);

    float key_slot_factor() const {
        return static_cast<float>(core_->GetSize()) /
               static_cast<float>(core_->GetSlotSize());
    }

    void CleanAll();

    MIOPair *GetNextRoom(const void *key);
    MIOPair *GetOrInsertRoom(const void *key, bool *insert);
    MIOPair *GetRoom(const void *key, int *slot);

    void Rehash(float scalar);

    template<class K, class V>
    inline MIOHashMapStub<K, V> *ToStub();
private:
    Handle<MIOHashMap> core_;
    ManagedAllocator *allocator_;
    Hash hash_;
    EqualTo equal_to_;

    int key_size_;
    int value_size_;
}; // class MIOHashMapSurface

template<class K, class V>
class MIOHashMapStubIterator {
public:
    inline explicit MIOHashMapStubIterator(MIOHashMapSurface *surface)
        : surface_(DCHECK_NOTNULL(surface)) {
        DCHECK(NativeValue<K>().Allow(surface->core()->GetKey()));
        DCHECK(NativeValue<V>().Allow(surface->core()->GetValue()));
    }

    inline void Init() { pair_ = surface_->GetNextRoom(nullptr); }

    inline bool HasNext() const { return pair_ != nullptr; }

    inline void MoveNext() {
        DCHECK(HasNext());
        pair_ = surface_->GetNextRoom(pair_->GetKey());
    }

    inline K key() const { return NativeValue<K>().Deref(pair_->GetKey()); }

    inline V value() const { return NativeValue<V>().Deref(pair_->GetValue()); }

private:
    MIOHashMapSurface *surface_;
    MIOPair *pair_ = nullptr;
};

template<class K, class V>
class MIOHashMapStub : public MIOHashMapSurface {
public:
    typedef MIOHashMapStubIterator<K, V> Iterator;

    inline MIOHashMapStub(MIOHashMap *core, ManagedAllocator *allocator)
        : MIOHashMapSurface(core, allocator) {}

    inline bool Put(K key, V value) {
        bool ok = true;
        return RawPut(NativeValue<K>().Address(&key),
                      NativeValue<V>().Address(&value), &ok);
    }

    inline V Get(K key) {
        auto addr = RawGet(NativeValue<K>().Address(&key));
        return addr ? NativeValue<V>().Deref(addr) : V(0);
    }

    inline bool Exist(K key) {
        int index;
        return GetRoom(NativeValue<K>().Address(&key), &index) != nullptr;
    }

    inline K GetFirstKey(bool *exist) {
        auto pair = GetNextRoom(nullptr);
        *exist = pair != nullptr;
        return *exist ? NativeValue<K>().Deref(pair->GetKey()) : K(0);
    }

    inline K GetNextKey(K key, bool *exist) {
        auto pair = GetNextRoom(NativeValue<K>().Address(&key));
        *exist = pair != nullptr;
        return *exist ? NativeValue<K>().Deref(pair->GetKey()) : K(0);
    }

    inline bool Delete(K key) {
        return RawDelete(NativeValue<K>().Address(&key));
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOHashMapStub)
}; // class MIOHashMapStub

class MIOArraySurface {
public:
    MIOArraySurface(Handle<HeapObject> ob, ManagedAllocator *allocator);

    DEF_GETTER(Handle<MIOVector>, core)
    DEF_GETTER(int, element_size)

    int size() const { return slice_.empty() ? core_->GetSize() : slice_->GetRangeSize(); }

    MIOReflectionType *element() { return core_->GetElement(); }

    inline void *RawGet(mio_int_t index) {
        return static_cast<uint8_t*>(core_->GetData()) + (index + begin_) * element_size_;
    }

    void *AddRoom(mio_int_t incr, bool *ok);

    template<class T>
    inline MIOArrayStub<T> *ToStub();

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOArraySurface)
private:

    Handle<MIOSlice>  slice_;
    Handle<MIOVector> core_;
    int               begin_;
    ManagedAllocator *allocator_;
    int               element_size_;
}; // class MIOArraySurface


template<class T>
class MIOArrayStub : public MIOArraySurface {
public:
    inline MIOArrayStub(Handle<HeapObject> ob, ManagedAllocator *allocator)
        : MIOArraySurface(ob, allocator) {
        DCHECK(NativeValue<T>().Allow(core()->GetElement()));
    }

    inline void Set(int index, T value) {
        FastMemoryMove(RawGet(index), NativeValue<T>().Address(&value),
                       element_size());
    }

    inline T Get(int index) {
        return NativeValue<T>().Deref(RawGet(index));
    }

    inline void Add(T value, bool *ok) {
        auto p = AddRoom(1, ok);
        if (!*ok) {
            return;
        }
        FastMemoryMove(p, NativeValue<T>().Address(&value), element_size());
    }

    inline void Reserve(int incr, bool *ok) { AddRoom(incr, ok); }

}; // class MIOArrayStub

class MIOExternalStub {
public:
    template<class T>
    static inline T *RawGet(MIOExternal *ex) {
        return static_cast<T *>(ex->GetValue());
    }

    template<class T>
    static inline T *Get(MIOExternal *ex) {
        ExternalGenerator<T> generator;
        return ex->GetTypeCode() == generator.type_code() ?
               RawGet<T>(ex) : nullptr;
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOExternalStub)
}; // class MIOArraySurface

template<class K, class V>
inline MIOHashMapStub<K, V> *MIOHashMapSurface::ToStub() {
    static_assert(sizeof(MIOHashMapStub<K, V>) == sizeof(*this), "do not allow to stub");

    if (!NativeValue<K>().Allow(core_->GetKey())) {
        return nullptr;
    }
    if (!NativeValue<V>().Allow(core_->GetValue())) {
        return nullptr;
    }
    return static_cast<MIOHashMapStub<K, V> *>(this);
}

template<class T>
inline MIOArrayStub<T> *MIOArraySurface::ToStub() {
    static_assert(sizeof(MIOArrayStub<T>) == sizeof(*this), "do not allow to stub");

    if (!NativeValue<T>().Allow(core_->GetElement())) {
        return nullptr;
    }
    return static_cast<MIOArrayStub<T> *>(this);
}

} // namespace mio

#endif // MIO_VM_OBJECT_SURFACE_H_
