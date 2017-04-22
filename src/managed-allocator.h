#ifndef MIO_MANAGED_ALLOCATOR_H_
#define MIO_MANAGED_ALLOCATOR_H_

#include <stdio.h>
#include <stdlib.h>

namespace mio {

class ManagedAllocator {
public:
    void *Allocate(int size) { return ::malloc(size); }

    void Free(const void *p) { ::free(const_cast<void *>(p)); }

}; // class ManagedAllocator

} // namespace mio

#endif // MIO_MANAGED_ALLOCATOR_H_
