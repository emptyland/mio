#ifndef VM_CODE_CACHE_H_
#define VM_CODE_CACHE_H_

#include "base.h"
#include "glog/logging.h"

namespace mio {

class CodeRef {
public:
    CodeRef(void **index)
        : index_(index) {}

    CodeRef(const CodeRef &) = default;
    CodeRef &operator = (const CodeRef &) = default;

    void *data() const { return DCHECK_NOTNULL(*index_); }

    void **index() const { return index_; }

    bool empty() const { return index_ == nullptr; }

private:
    void **index_;
};

class CodeCache {
public:
    CodeCache(int default_size) : size_(default_size) {}
    ~CodeCache();

    int chunk_size() const { return static_cast<int>(index_ - code_); }

    bool Init();

    CodeRef Allocate(int size);

    void Free(CodeRef ref);

    int FindFirstOne(int begin);

    void **MakeIndexRoom();

    void Compact();

    bool bitmap_test(int index) {
        return (bitmap_[index / 32] & (1u << (index % 32))) != 0;
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(CodeCache)
private:
    void MarkUsed(void *chunk, int size) {
        auto offset = static_cast<int>(static_cast<uint8_t *>(chunk) - code_);
        bitmap_set(offset / kAlignmentSize);
        bitmap_set((offset + size) / kAlignmentSize);
    }

    void MarkUnused(void *chunk);

    int bitmap_size() const { return ((size_ / kAlignmentSize) + 31) / 32; }

    void bitmap_set(int index) {
        bitmap_[index / 32] |= (1u << (index % 32));
    }

    void bitmap_unset(int index) {
        bitmap_[index / 32] &= ~(1u << (index % 32));
    }

    uint8_t  *code_       = nullptr;
    int       size_;
    uint32_t *bitmap_     = nullptr;
    uint8_t  *index_free_ = nullptr;
    uint8_t  *index_      = nullptr;
};

} // namespace mio

#endif // VM_CODE_CACHE_H_
