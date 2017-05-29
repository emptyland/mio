#ifndef MIO_FALLBACK_MANAGED_ALLOCATOR_H_
#define MIO_FALLBACK_MANAGED_ALLOCATOR_H_

#include "managed-allocator.h"

namespace mio {

class FallbackManagedAllocator : public ManagedAllocator {
public:
    FallbackManagedAllocator(bool running_count)
        : running_count_(running_count) {}

    virtual ~FallbackManagedAllocator() override;

    virtual bool Init() override;
    virtual void Finialize() override;
    virtual void *Allocate(int size) override;
    virtual void Free(const void *p) override;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FallbackManagedAllocator)
private:
    bool running_count_ = false;
    int *chunk_count_ = nullptr;
    int  chunk_count_size_ = 0;
};

} // namespace mio

#endif // MIO_FALLBACK_MANAGED_ALLOCATOR_H_
