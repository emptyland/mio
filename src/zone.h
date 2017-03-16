#ifndef MIO_ZONE_H_
#define MIO_ZONE_H_

#include "base.h"

namespace mio {

class ZonePage;
class ZoneSlab;

// slots:
// [0] 16
// [1] 32
// [2] 64
// [3] 128
// [4] 256
// [5] 512
// [6] 1024
// [7] 2048
class Zone {
public:
    static const int kNumberOfSlabs     = 8;

    static const uintptr_t kPageAlignmentMask = ~(kPageSize - 1);

    static const int kMinAllocatedShift = 4;
    static const int kMinAllocatedSize  = (1 << kMinAllocatedShift);

    static const int kDefaultMaxCacheBytes = kPageSize * 4;

    static const int64_t kInitialSeed = 1315423911;

    Zone() : Zone(kInitialSeed) {}
    explicit Zone(int64_t seed);
    ~Zone();

    DEF_PROP_RW(size_t, max_cache_bytes)
    DEF_PROP_RW(size_t, keeped_cache_bytes)

    int64_t generated_id() { return seed_; }

    size_t slab_chunk_size(int index) const;
    int slab_max_chunks(int index) const;

    void *Allocate(size_t size);
    void Free(const void *p);

    void AssertionTest();
    void PreheatEverySlab();

    void GenerateUniqueId() {
        seed_ ^= ((seed_ << 5) + (sequence_id_++) + (seed_ >> 2));
    }

    void TEST_Report();

    typedef ZonePage Page;
    typedef ZoneSlab Slab;
private:
    Slab *slabs_ = nullptr;
    int64_t sequence_id_ = 0;
    int64_t seed_;
    size_t max_cache_bytes_ = kDefaultMaxCacheBytes;
    size_t keeped_cache_bytes_ = kDefaultMaxCacheBytes / 2;
}; // class Zone


class ManagedObject {
public:
    ManagedObject() {}
    ~ManagedObject() {}

    void *operator new (size_t size, Zone *z) { return z->Allocate(size); }

    void operator delete (void *p, Zone *z)  { return z->Free(p); }
    void operator delete (void *p, size_t size) = delete;

}; // class ManagedObject

} // namespace mio

#endif // MIO_ZONE_H_
