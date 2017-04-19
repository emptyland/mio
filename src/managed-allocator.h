#ifndef MIO_MANAGED_ALLOCATOR_H_
#define MIO_MANAGED_ALLOCATOR_H_

#include <stdio.h>
#include <stdlib.h>

namespace mio {

class ManagedAllocator {
public:
    void *Allocate(int size) { return ::malloc(size); }

    void Free(const void *p) { ::free(const_cast<void *>(p)); }

    template<class T>
    inline T *NewObject(int placement_size) {
        auto ob = static_cast<T *>(Allocate(placement_size));
        ob->Init(static_cast<HeapObject::Kind>(T::kSelfKind));
        return ob;
    }

    void DeleteObject(const HeapObject *ob) {
        printf("delete object: %p\n", ob);
        Free(ob);
    }
}; // class ManagedAllocator

} // namespace mio

#endif // MIO_MANAGED_ALLOCATOR_H_
