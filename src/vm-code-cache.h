#ifndef VM_CODE_CACHE_H_
#define VM_CODE_CACHE_H_

#include "base.h"
#include "glog/logging.h"
#include <vector>
#include <map>

namespace mio {

class CodeRef {
public:
    CodeRef(void **index)
        : index_(index) {}

    CodeRef(const CodeRef &) = default;
    CodeRef &operator = (const CodeRef &) = default;

    void *data() const { return DCHECK_NOTNULL(*index_); }

    uint8_t *data(int index) { return static_cast<uint8_t *>(data()) + index; }

    void **index() const { return index_; }

    bool empty() const { return index_ == nullptr; }

    bool null() const { return !*DCHECK_NOTNULL(index_); }

private:
    void **index_;
};

class CodeCache {
public:
    CodeCache(int default_size) : size_(default_size) {}
    ~CodeCache();

    bool Init();

    DEF_GETTER(int, used_bytes)

    int space_size() const { return static_cast<int>(index_ - code_); }

    inline CodeRef Allocate(int size);

    void **RawAllocate(int size);

    void Free(CodeRef ref);

    void **MakeIndexRoom();

    void Compact();

    bool bitmap_test(int index) const {
        return (bitmap_[index / 32] & (1u << (index % 32))) != 0;
    }

    int GetChunkSize(void *chunk) const;

    void GetAllChunks(std::vector<mio_buf_t<uint8_t>> *chunks) const;

    void GetIndexMap(std::map<void *, void **> *indexs) const;

    DISALLOW_IMPLICIT_CONSTRUCTORS(CodeCache)
private:
    int FindFirstOne(int begin) const;
    
    inline void MarkUsed(void *chunk, int size);

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
    int       used_bytes_ = 0;
    uint32_t *bitmap_     = nullptr;
    uint8_t  *index_free_ = nullptr;
    uint8_t  *index_      = nullptr;
}; // CodeCache

inline CodeRef CodeCache::Allocate(int size) {
    auto index = RawAllocate(size);
    if (!index && size <= space_size() - used_bytes_) {
        Compact();
        index = RawAllocate(size);
    }
    return CodeRef(index);
}

inline void CodeCache::MarkUsed(void *chunk, int size) {
    auto offset = static_cast<int>(static_cast<uint8_t *>(chunk) - code_);
    bitmap_set(offset >> kAlignmentSizeShift);
    bitmap_set((offset + size - 1) >> kAlignmentSizeShift);
}

} // namespace mio

#endif // VM_CODE_CACHE_H_
