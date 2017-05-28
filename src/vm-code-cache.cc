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

void **CodeCache::RawAllocate(int size) {
    if (size <= kAlignmentSize) {
        size = kAlignmentSize * 2;
    }
    size = RoundUp(size, kAlignmentSize);

    int      begin = FindFirstOne(0);
    uint8_t *free = code_;
    int      free_size = space_size() - sizeof(void *);

    while ((begin << kAlignmentSizeShift) < space_size()) {
        begin = FindFirstOne(begin + 1) + 1;
        free = code_ + (begin << kAlignmentSizeShift);
        auto end = FindFirstOne(begin);
        free_size = (end - begin) << kAlignmentSizeShift;

        if (size <= free_size) {
            break;
        }
    }

    if (size > free_size) {
        return nullptr;
    }
    auto index_room = MakeIndexRoom();
    if (!index_room) {
        return nullptr;
    }
    *index_room = free;
    MarkUsed(free, size);
    used_bytes_ += size;
    return index_room;
}

void CodeCache::Free(CodeRef ref) {
    used_bytes_ -= GetChunkSize(ref.data());

    MarkUnused(ref.data());

    *ref.index() = nullptr;
    if (reinterpret_cast<uint8_t *>(ref.index()) > index_free_) {
        index_free_ = reinterpret_cast<uint8_t *>(ref.index());
    }
}

int CodeCache::FindFirstOne(int begin) const {
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
    std::vector<mio_buf_t<uint8_t>> chunks;
    GetAllChunks(&chunks);

    std::map<void *, void **> indexs;
    GetIndexMap(&indexs);

    auto p = code_;
    for (auto buf : chunks) {
        if (buf.z > p) {
            MarkUnused(buf.z);
            memmove(p, buf.z, buf.n);
            MarkUsed(p, buf.n);
            *indexs[buf.z] = p;
            
            p += buf.n;
        } else {
            p = buf.z + buf.n;
        }
    }
}

void CodeCache::GetAllChunks(std::vector<mio_buf_t<uint8_t>> *chunks) const {
    int begin = FindFirstOne(0);

    while ((begin << kAlignmentSizeShift) < space_size()) {
        mio_buf_t<uint8_t> buf;
        buf.z = code_ + (begin << kAlignmentSizeShift);

        auto end = FindFirstOne(begin + 1) + 1;
        buf.n = (end - begin) << kAlignmentSizeShift;

        chunks->push_back(buf);
        begin = FindFirstOne(end);
    }
}

void CodeCache::GetIndexMap(std::map<void *, void **> *indexs) const {
    for (auto p = index_; p < code_ + size_; p += sizeof(void *)) {
        if (!*reinterpret_cast<void **>(p)) {
            continue;
        }
        indexs->emplace(*reinterpret_cast<void **>(p),
                        reinterpret_cast<void **>(&code_[p - code_]));
    }
}

int CodeCache::GetChunkSize(void *chunk) const {
    int offset = static_cast<int>((static_cast<uint8_t *>(chunk) - code_));
    DCHECK_GE(offset, 0);

    auto begin = offset >> kAlignmentSizeShift;
    DCHECK(bitmap_test(begin));

    auto end   = FindFirstOne(begin + 1);
    DCHECK_LT(end * kAlignmentSize, space_size());

    return (end - begin + 1) << kAlignmentSizeShift;
}

void CodeCache::MarkUnused(void *chunk) {
    int offset = static_cast<int>((static_cast<uint8_t *>(chunk) - code_));
    DCHECK_GE(offset, 0);

    auto begin = offset >> kAlignmentSizeShift;
    DCHECK(bitmap_test(begin));

    auto end   = FindFirstOne(begin + 1);
    DCHECK_LT(end * kAlignmentSize, space_size());

    bitmap_unset(begin);
    bitmap_unset(end);
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
