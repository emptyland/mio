#ifndef MIO_VM_MEMORY_SEGMENT_H_
#define MIO_VM_MEMORY_SEGMENT_H_

#include "base.h"
#include "glog/logging.h"
#include <stdint.h>

namespace mio {

class MemorySegment {
public:
    MemorySegment();
    ~MemorySegment();

    DEF_GETTER(int, size)
    DEF_GETTER(int, capacity)

    void *offset(int p) {
        DCHECK_GE(p, 0);
        DCHECK_LT(p, size_);
        return static_cast<void *>(bytes() + p);
    }

    void *base() { return static_cast<void *>(bytes()); }

    void *Advance(int add);
    void *AlignAdvance(int add) {
        return Advance(static_cast<int>(AlignDownBounds(kAlignmentSize, add)));
    }

    template<class T>
    void Add(T value) {
        *static_cast<T *>(AlignAdvance(sizeof(value))) = value;
    }

    template<class T>
    T Get(int addr) {
        return *static_cast<T *>(offset(addr));
    }

private:
    uint8_t *bytes() const { return static_cast<uint8_t *>(chunk_); }

    void *chunk_;
    int   size_;
    int   capacity_;
};

} // namespace mio

#endif // MIO_VM_MEMORY_SEGMENT_H_
