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

    static const int kPageShift         = 12;
    static const int kPageSize          = (1 << kPageShift); // 4k
    static const uintptr_t kPageAlignmentMask = ~(kPageSize - 1);

    static const int kAlignmentSize     = 4;

    static const int kMinAllocatedShift = 4;
    static const int kMinAllocatedSize  = (1 << kMinAllocatedShift);

    static const int kDefaultMaxCacheBytes = kPageSize * 4;

    Zone();
    ~Zone();

    DEF_PROP_RW(size_t, max_cache_bytes)
    DEF_PROP_RW(size_t, keeped_cache_bytes)

    size_t slab_chunk_size(int index) const;
    int slab_max_chunks(int index) const;

    void *Allocate(size_t size);
    void Free(const void *p);

    void AssertionTest();
    void PreheatEverySlab();

    void TEST_Report();

    typedef ZonePage Page;
    typedef ZoneSlab Slab;
private:
    Slab *slabs_ = nullptr;
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
