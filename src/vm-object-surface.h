#ifndef MIO_VM_OBJECT_SURFACE_H_
#define MIO_VM_OBJECT_SURFACE_H_

#include "vm-objects.h"
#include "vm-runtime.h"
#include "managed-allocator.h"
#include "glog/logging.h"

namespace mio {

template<class K, class V> class MIOHashMapStub;

class MIOHashMapSurface {
public:
    constexpr static const float kRehashTopFactor    = 1.3f;
    constexpr static const float kRehashBottomFactor = 0.3f;
    static const int kMinSloSize = 7;

    typedef int (*Hash)(const void *, int);
    typedef bool (*EqualTo)(mio_buf_t<const void>, mio_buf_t<const void>);

    MIOHashMapSurface(MIOHashMap *core, ManagedAllocator *allocator);

    DEF_GETTER(Handle<MIOHashMap>, core)

    bool RawPut(const void *key, const void *value) {
        bool insert = false;
        auto pair = GetOrInsertRoom(key, &insert);
        memcpy(pair->GetValue(), value, value_size_);
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

namespace detail {

template<class T>
struct Traits {
    inline const void *Address(T *value) {
        return static_cast<const void *>(value);
    }

    inline T Deref(void *addr) {
        return *static_cast<T *>(addr);
    }

    inline bool Allow(MIOReflectionType *) {
        return false;
    }
}; // struct Traits

template<>
struct Traits<mio_i8_t> {
    inline const void *Address(mio_i8_t *value) {
        return static_cast<const void *>(value);
    }

    inline mio_i8_t Deref(void *addr) {
        return *static_cast<mio_i8_t *>(addr);
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionIntegral() &&
               type->AsReflectionIntegral()->GetBitWide() == 8;
    }
}; // struct Traits<mio_i8_t>

template<>
struct Traits<mio_i32_t> {
    inline const void *Address(mio_i32_t *value) {
        return static_cast<const void *>(value);
    }

    inline mio_i32_t Deref(void *addr) {
        return *static_cast<mio_i32_t *>(addr);
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionIntegral() &&
               type->AsReflectionIntegral()->GetBitWide() == 32;
    }
}; // struct Traits<mio_i32_t>

// TODO: other types

template<>
struct Traits<Handle<MIOString>> {
    inline const void *Address(Handle<MIOString> *value) {
        return static_cast<const void *>(value->address());
    }

    inline Handle<MIOString> Deref(void *addr) {
        return make_handle(*static_cast<MIOString **>(addr));
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionString();
    }
}; // struct Traits<Handle<MIOString>>

template<>
struct Traits<Handle<MIOError>> {
    inline const void *Address(Handle<MIOError> *value) {
        return static_cast<const void *>(value->address());
    }

    inline Handle<MIOError> Deref(void *addr) {
        return make_handle(*static_cast<MIOError **>(addr));
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionError();
    }
}; // struct Traits<Handle<MIOError>>

} // namespace detail

namespace d = detail;

template<class K, class V>
class MIOHashMapStubIterator {
public:
    inline explicit MIOHashMapStubIterator(MIOHashMapSurface *surface)
        : surface_(DCHECK_NOTNULL(surface)) {
        DCHECK(d::Traits<K>().Allow(surface->core()->GetKey()));
        DCHECK(d::Traits<V>().Allow(surface->core()->GetValue()));
    }

    inline void Init() { pair_ = surface_->GetNextRoom(nullptr); }

    inline bool HasNext() const { return pair_ != nullptr; }

    inline void MoveNext() {
        DCHECK(HasNext());
        pair_ = surface_->GetNextRoom(pair_->GetKey());
    }

    inline K key() const { return d::Traits<K>().Deref(pair_->GetKey()); }

    inline V value() const { return d::Traits<V>().Deref(pair_->GetValue()); }

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
        return RawPut(d::Traits<K>().Address(&key),
                      d::Traits<V>().Address(&value));
    }

    inline V Get(K key) {
        auto addr = RawGet(d::Traits<K>().Address(&key));
        return addr ? d::Traits<V>().Deref(addr) : V(0);
    }

    inline bool Exist(K key) {
        int index;
        return GetRoom(d::Traits<K>().Address(&key), &index) != nullptr;
    }

    inline K GetFirstKey(bool *exist) {
        auto pair = GetNextRoom(nullptr);
        *exist = pair != nullptr;
        return *exist ? d::Traits<K>().Deref(pair->GetKey()) : K(0);
    }

    inline K GetNextKey(K key, bool *exist) {
        auto pair = GetNextRoom(d::Traits<K>().Address(&key));
        *exist = pair != nullptr;
        return *exist ? d::Traits<K>().Deref(pair->GetKey()) : K(0);
    }

    inline bool Delete(K key) {
        return RawDelete(d::Traits<K>().Address(&key));
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOHashMapStub)
}; // class MIOHashMapStub

template<class K, class V>
inline MIOHashMapStub<K, V> *MIOHashMapSurface::ToStub() {
    static_assert(sizeof(MIOHashMapStub<K, V>) == sizeof(*this), "do not allow to stub");

    if (!d::Traits<K>().Allow(core_->GetKey())) {
        return nullptr;
    }
    if (!d::Traits<V>().Allow(core_->GetValue())) {
        return nullptr;
    }
    return static_cast<MIOHashMapStub<K, V> *>(this);
}

} // namespace mio

#endif // MIO_VM_OBJECT_SURFACE_H_
