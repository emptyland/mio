#ifndef MIO_VM_OBJECT_SURFACE_H_
#define MIO_VM_OBJECT_SURFACE_H_

#include "vm-objects.h"
#include "vm-runtime.h"
#include "managed-allocator.h"

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
        auto pair = GetRoom(key);
        return pair ? pair->GetValue() : nullptr;
    }

    MIOPair *GetOrInsertRoom(const void *key, bool *insert);
    MIOPair *GetRoom(const void *key);

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

} // namespace detail

template<class K, class V>
class MIOHashMapStub : public MIOHashMapSurface {
public:
    bool Put(K key, V value) {
        return RawPut(detail::Traits<K>().Address(&key),
                      detail::Traits<V>().Address(&value));
    }

    V Get(K key) {
        return detail::Traits<V>().Deref(RawGet(detail::Traits<K>().Address(&key)));
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOHashMapStub)
}; // class MIOHashMapStub

template<class K, class V>
inline MIOHashMapStub<K, V> *MIOHashMapSurface::ToStub() {
    static_assert(sizeof(MIOHashMapStub<K, V>) == sizeof(*this), "do not allow to stub");

    if (!detail::Traits<K>().Allow(core_->GetKey())) {
        return nullptr;
    }
    if (!detail::Traits<V>().Allow(core_->GetValue())) {
        return nullptr;
    }
    return static_cast<MIOHashMapStub<K, V> *>(this);
}

} // namespace mio

#endif // MIO_VM_OBJECT_SURFACE_H_
