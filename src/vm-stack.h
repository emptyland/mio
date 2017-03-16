#ifndef MIO_VM_STACK_H_
#define MIO_VM_STACK_H_

#include "base.h"

namespace mio {

class Stack {
public:
    void *offset(int delta) const { return static_cast<void *>(base_ + delta); }

    void AdjustBase(int delta) {
        base_ += delta;
        top_   = base_;
    }

    void *Advance(int add) {
        // TODO:
        return nullptr;
    }

    void *AlignAdvance(int add) {
        return Advance(static_cast<int>(AlignDownBounds(kAlignmentSize, add)));
    }

    template<class T>
    void *Push(T value) {
        *top<T>() = value;
        return AlignAdvance(sizeof(value));
    }

private:
    template<class T>
    T *top() const { reinterpret_cast<T *>(top_); }

    uint8_t *bytes() const { return static_cast<uint8_t *>(chunk_); }

    void *chunk_;
    uint8_t *base_;
    uint8_t *top_;
    int   capacity_;
}; // class Stack

} // namespace mio

#endif // MIO_VM_STACK_H_
