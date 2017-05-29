#ifndef MIO_MANAGED_ALLOCATOR_H_
#define MIO_MANAGED_ALLOCATOR_H_

#include "base.h"

namespace mio {

class ManagedAllocator {
public:
    ManagedAllocator() = default;
    virtual ~ManagedAllocator() = default;

    /**
     * Initialize allocator
     */
    virtual bool Init() = 0;

    /**
     * Cleanup allocator
     */
    virtual void Finialize() = 0;

    /**
     * Allocate memory from managed area
     */
    virtual void *Allocate(int size) = 0;

    /**
     * Free memory to managed area
     */
    virtual void Free(const void *p) = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(ManagedAllocator)
}; // class ManagedAllocator

} // namespace mio

#endif // MIO_MANAGED_ALLOCATOR_H_
