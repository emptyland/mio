#ifndef MIO_VM_STACK_H_
#define MIO_VM_STACK_H_

#include "base.h"
#include "glog/logging.h"
#include <string.h>

namespace mio {

class Stack {
public:
    static const int kDefaultSize = kPageSize;

    Stack();
    ~Stack();

    void *offset(int delta) const { return static_cast<void *>(base_ + delta); }

    int size() const { return static_cast<int>(top_ - base_); }
    int base_size() const { return static_cast<int>(base_ - bytes()); }
    int total_size() const { return static_cast<int>(top_ - bytes()); }

    inline void AdjustFrame(int delta, int size);
    inline void SetFrame(int base, int size);
    inline void *Advance(int add);

    void *AlignAdvance(int add) {
        return Advance(static_cast<int>(AlignDownBounds(kAlignmentSize, add)));
    }

    void ResizeIfNeeded(int add);

    template<class T>
    void Push(T value) {
        *static_cast<T *>(AlignAdvance(sizeof(value))) = value;
    }

    void Push(const void *p, int n) {
        ::memcpy(AlignAdvance(n), p, n);
    }

    template<class T>
    void Set(int delta, T value) {
        //DCHECK_GE(delta, 0);
        DCHECK_LT(delta, size());
        *static_cast<T *>(offset(delta)) = value;
    }

    template<class T>
    T Get(int delta) {
        //DCHECK_GE(delta, 0);
        DCHECK_LT(delta, size());
        return *static_cast<T *>(offset(delta));
    }

    template<class T>
    T *top() const { return reinterpret_cast<T *>(top_); }

private:
    uint8_t *bytes() const { return static_cast<uint8_t *>(chunk_); }

    void *chunk_;
    uint8_t *base_;
    uint8_t *top_;
    int   capacity_;
}; // class Stack


inline void Stack::AdjustFrame(int delta, int size) {
    ResizeIfNeeded(delta + size);
    base_ += delta;
    top_   = base_ + size;
}

inline void Stack::SetFrame(int base, int size) {
    ResizeIfNeeded((total_size() - base) + size);
    base_ = bytes() + base;
    top_  = base_  + size;
}

inline void *Stack::Advance(int add) {
    ResizeIfNeeded(add);
    auto rv = static_cast<void *>(top_);
    top_ += add;
    return rv;
}

} // namespace mio

#endif // MIO_VM_STACK_H_
