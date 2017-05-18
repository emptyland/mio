#include "vm-code-cache.h"
#include "bit-operations.h"
#include <sys/mman.h>

namespace mio {

CodeCache::~CodeCache() {
    delete[] bitmap_;
    if (code_) {
        munmap(code_, size_);
    }
}
    
bool CodeCache::Init() {
    auto bounded_size = RoundUp(size_, kPageSize);
    
    code_ = static_cast<uint8_t*>(mmap(nullptr, bounded_size,
                                       PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE,
                                       -1, 0));
    if (!code_) {
        return false;
    }
    size_ = bounded_size;

    for (auto p = code_; p < code_ + size_; p += kPageSize) {
        if (mprotect(p, kPageSize, PROT_EXEC|PROT_READ|PROT_WRITE) != 0) {
            PLOG(ERROR) << "mprotect fail";
            munmap(code_, size_);
            return false;
        }
    }

    auto bmp_size = bitmap_size();
    bitmap_ = new uint32_t[bmp_size];
    memset(bitmap_, 0, bmp_size * sizeof(*bitmap_));

    index_      = (code_ + size_);
    index_free_ = index_;
    return true;
}

CodeRef CodeCache::Allocate(int size) {
    size = RoundUp(size, kAlignmentSize);

    int      begin = 0, end = 0;
    uint8_t *free = nullptr;
    int      free_size = 0;

    do {
        begin = FindFirstOne(end);
        if (begin >= chunk_size() / kAlignmentSize) {
            free = code_;
            free_size = chunk_size();
        } else {
            begin = FindFirstOne(begin + 1);
            free = code_ + begin * kAlignmentSize;
            end = FindFirstOne(begin + 1);
            free_size = (end - begin) * kAlignmentSize;
        }
        if (size <= free_size) {
            auto index_room = MakeIndexRoom();
            if (!index_room) {
                return CodeRef(nullptr);
            }
            *index_room = free;
            MarkUsed(free, size);
            return CodeRef(index_room);
        }
    } while (begin < chunk_size());
    return CodeRef(nullptr);
}

void CodeCache::Free(CodeRef ref) {
    MarkUnused(ref.data());

    *ref.index() = nullptr;
    if (reinterpret_cast<uint8_t *>(ref.index()) > index_free_) {
        index_free_ = reinterpret_cast<uint8_t *>(ref.index());
    }
}

int CodeCache::FindFirstOne(int begin) {
    int index = begin / 32;
    DCHECK_GE(index, 0);
    DCHECK_LT(index, bitmap_size());

    int result = index * 32;
    int bits = 0;
    for (bits = begin % 32; bits < 32; ++bits) {
        if (((1u << bits) & bitmap_[index]) != 0) {
            break;
        }
    }
    result += bits;
    if (bits < 32) {
        return result;
    }
    for (int i = index + 1; i < bitmap_size(); ++i) {
        bits = Bits::FindFirstOne32(bitmap_[i]);
        result += bits;
        if (bits < 32) {
            break;
        }
    }
    return result;
}

void CodeCache::Compact() {
    // TODO:
}

void CodeCache::MarkUnused(void *chunk) {
    int offset = static_cast<int>((static_cast<uint8_t *>(chunk) - code_));
    DCHECK_GE(offset, 0);
    DCHECK(bitmap_test(offset / kAlignmentSize));

    auto begin = offset / kAlignmentSize;
    auto end   = FindFirstOne(begin);
    DCHECK_LT(end * kAlignmentSize, chunk_size());

    bitmap_unset(begin); bitmap_unset(end);
}

void **CodeCache::MakeIndexRoom() {
    void **result = reinterpret_cast<void **>(&code_[index_free_ - code_]);

    while (index_free_ > index_) {
        if (*reinterpret_cast<void **>(index_free_) == nullptr) {
            break;
        }
        index_free_ -= sizeof(void *);
    }
    if (index_free_ == index_) {
        index_ -= sizeof(void *);
        index_free_ = index_;

        result = reinterpret_cast<void **>(&code_[index_free_ - code_]);
    }

    MarkUsed(result, sizeof(void *));
    return result;
}

} // namespace mio
