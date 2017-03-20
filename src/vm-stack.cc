#include "vm-stack.h"
#include <stdlib.h>

namespace mio {

Stack::Stack()
    : chunk_(::malloc(kDefaultSize))
    , base_(static_cast<uint8_t *>(chunk_))
    , top_(base_)
    , capacity_(kDefaultSize) {
}

Stack::~Stack() {
    free(chunk_);
}

void Stack::ResizeIfNeeded(int add) {
    auto end = bytes() + capacity_;

    if (top_ + add >= end) {
        auto base_delta = base_ - bytes();
        auto top_delta  = top_  - bytes();

        capacity_ += kPageSize;
        chunk_ = ::realloc(chunk_, capacity_);
        base_ = bytes() + base_delta;
        top_  = bytes() + top_delta;
    }
}

} // namespace mio
