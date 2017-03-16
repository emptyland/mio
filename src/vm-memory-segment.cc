#include "vm-memory-segment.h"

namespace mio {

MemorySegment::MemorySegment()
    : size_(0)
    , capacity_(kPageSize)
    , chunk_(malloc(kPageSize)) {
}

MemorySegment::~MemorySegment() {
    free(chunk_);
}

void *MemorySegment::Advance(int add) {
    if (size_ + add > capacity_) {
        capacity_ += kPageSize;
        chunk_ = realloc(chunk_, capacity_);
    }
    auto old = size_;
    size_ += add;
    return offset(old);
}

} // namespace mio
