#include "fallback-managed-allocator.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace mio {

/*virtual*/ FallbackManagedAllocator::~FallbackManagedAllocator() {
    delete[] chunk_count_;
}

/*virtual*/ bool FallbackManagedAllocator::Init() {
    if (running_count_) {
        chunk_count_size_ = kLargeSize / kAlignmentSize;
        chunk_count_ = new int[chunk_count_size_ + 1];
        memset(chunk_count_, 0, (chunk_count_size_ + 1) * sizeof(*chunk_count_));
    }
    return true;
}

/*virtual*/ void FallbackManagedAllocator::Finialize() {
    if (!running_count_) {
        return;
    }
    for (int i = 0; i < chunk_count_size_; ++i) {
        if (!chunk_count_[i]) {
            continue;
        }

        printf("-- size:[%d] --> %d == %d\n",
               i * kAlignmentSize,
               chunk_count_[i],
               chunk_count_[i] * kAlignmentSize);
    }
    printf("-- size:[large] --> %d\n", chunk_count_[chunk_count_size_]);
}

/*virtual*/ void *FallbackManagedAllocator::Allocate(int size) {
    if (running_count_) {
        if (size <= kLargeSize) {
            chunk_count_[(size + (kAlignmentSize - 1))/kAlignmentSize]++;
        } else {
            chunk_count_[chunk_count_size_] += size;
        }
    }
    return ::malloc(size);
}

/*virtual*/ void FallbackManagedAllocator::Free(const void *p) {
    ::free(const_cast<void *>(p));
}

} // namespace mio
