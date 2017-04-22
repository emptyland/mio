#ifndef MIO_VM_OBJECT_SURFACE_H_
#define MIO_VM_OBJECT_SURFACE_H_

#include "vm-objects.h"
#include "vm-runtime.h"
#include "managed-allocator.h"

namespace mio {

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

private:
    Handle<MIOHashMap> core_;
    ManagedAllocator *allocator_;
    Hash hash_;
    EqualTo equal_to_;

    int key_size_;
    int value_size_;
}; // class MIOHashMapSurface

} // namespace mio

#endif // MIO_VM_OBJECT_SURFACE_H_
