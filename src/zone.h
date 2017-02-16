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

    Zone();
    ~Zone();

    DEF_GETTER(size_t, allocated_bytes)
    DEF_PROP_RW(bool, dont_use_cache)

    size_t slab_chunk_size(int index) const;

    int slab_max_chunks(int index) const;

    void *Allocate(size_t size);
    void Free(const void *p);

    void AssertionTest();
    void PreheatEverySlab();

    typedef ZonePage Page;
    typedef ZoneSlab Slab;
private:
    Slab *slabs_;
    size_t allocated_bytes_ = 0;
    bool dont_use_cache_ = false;
}; // class Zone


class ManagedObject {
public:
    ManagedObject() {}
    ~ManagedObject();

    void *operator new (size_t size, Zone *z) { return z->Allocate(size); }

    void operator delete (void *p, Zone *z)  { return z->Free(p); }
    void operator delete (void *p, size_t size) = delete;

}; // class ManagedObject

} // namespace mio

#endif // MIO_ZONE_H_
