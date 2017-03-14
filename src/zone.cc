#include "zone.h"
#include "glog/logging.h"
#include "internal-list.h"
#include <list>

#define SLAB_BITS(slab)  ((( slab ) & 0xffff0000) >> 16)
#define SLAB_SHIFT(slab) (( slab ) & 0x0000ffff)
#define MAKE_SLAB(bits, shift) \
    (((( bits ) & 0xffff) << 16) | (( shift ) & 0xffff))

namespace mio {

namespace {

#if defined(DEBUG) || defined(_DEBUG)
void *ZagBytesFill(const uint32_t zag, void *chunk, size_t n) {
    return Round32BytesFill(zag, chunk, n);
}
#else
inline void *ZagBytesFill(const uint32_t zag, void *chunk, size_t n) {
    (void)zag;
    (void)n;
    return chunk;
}
#endif

static const uint32_t kZoneInitialBytes = 0xcccccccc;
static const uint32_t kZoneFreeBytes    = 0xfeedfeed;

} // namespace

static_assert(Zone::kMinAllocatedSize >= sizeof(List::Entry),
              "Zone::kMinAllocatedSize too small.");

class ZonePage : public List::Entry {
public:
    void *payload() {
        return static_cast<void *>(this + 1);
    }

    void *payload_offset(size_t offset) {
        return static_cast<uint8_t *>(payload()) + offset;
    }

    uint32_t *bitmap() {
        return static_cast<uint32_t *>(payload());
    }

    DEF_PROP_RW(uint32_t, slab);

    static ZonePage *AlignmentGet(const void *p) {
        return static_cast<ZonePage *>(
            reinterpret_cast<void *>(
                reinterpret_cast<uintptr_t>(p) & Zone::kPageAlignmentMask));
    }

private:
    uint32_t  slab_;
}; // class ZonePage

class ZoneSlab {
public:

    enum Kind {
        PAGE_SMALL,
        PAGE_NORMAL,
        PAGE_LARGE,
    };

    ZoneSlab() {}

    DEF_GETTER(int, max_chunks)
    DEF_GETTER(size_t, chunk_size)
    DEF_GETTER(int, bitmap_bytes)
    DEF_GETTER(size_t, cached_size)

    bool is_cache_not_empty() const { return List::IsNotEmpty(&cache_); }

    void Init(size_t chunk_size);
    inline void Clear();

    void *Allocate(int shift);

    inline void *HitCache();
    inline size_t HitCache(void *chunk);
    inline size_t PurgeCache(size_t keeped_size);

private:
    void *AllocateFromPage(ZonePage *page, bool *full);
    void FreeToPage(ZonePage *page, const void *p);

    ZonePage *NewPage(int shift);

    void ClearOne(List::Entry *pages);

    List::Entry partial_;
    List::Entry full_;
    List::Entry empty_;

    Kind kind_;
    int max_chunks_;
    size_t chunk_size_;
    int bitmap_bytes_;

    List::Entry cache_;
    size_t  cached_size_ = 0;

    int allocated_chunks_ = 0;
}; // class ZoneSlab

void ZoneSlab::Init(size_t chunk_size) {
    max_chunks_ = (Zone::kPageSize - sizeof(ZonePage)) / chunk_size;
    chunk_size_ = chunk_size;
    bitmap_bytes_ = (max_chunks_ + 8) / 8;

    if (bitmap_bytes_ >= 2) {
        kind_ = PAGE_SMALL;
        bitmap_bytes_ = bitmap_bytes_ < 4 ? 4 : bitmap_bytes_;
        max_chunks_ = (Zone::kPageSize - sizeof(ZonePage) - bitmap_bytes_) / chunk_size;
    } else {
        kind_ = PAGE_NORMAL;
        bitmap_bytes_ = 0; // normal page's bitmap in slab member
    }

    cached_size_ = 0;
    allocated_chunks_ = 0;

    List::Init(&partial_);
    List::Init(&full_);
    List::Init(&empty_);
    List::Init(&cache_);
}

inline void ZoneSlab::Clear() {
    ClearOne(&partial_);
    ClearOne(&empty_);
    ClearOne(&full_);
}

void ZoneSlab::ClearOne(List::Entry *pages) {
    while (List::IsNotEmpty(pages)) {
        auto header = List::Head<ZonePage>(pages);
        List::Remove(header);
        free(header);
    }
}

void *ZoneSlab::Allocate(int shift) {
    if (List::IsNotEmpty(&partial_)) {
        auto page = List::Head<ZonePage>(&partial_);

        bool full = false;
        auto chunk = AllocateFromPage(page, &full);
        if (full) {
            List::Remove(page);
            List::InsertHead(&full_, page);
        }
        ++allocated_chunks_;
        return chunk;
    }

    if (List::IsNotEmpty(&empty_)) {
        auto page = List::Head<ZonePage>(&empty_);

        bool full = false;
        auto chunk = AllocateFromPage(page, &full);
        List::Remove(page);
        List::InsertHead(&partial_, page);
        return chunk;
    }

    ZonePage *page = NewPage(shift);
    if (!page) {
        return nullptr;
    }
    bool full = false;
    auto chunk = AllocateFromPage(page, &full);
    List::InsertHead(&partial_, page);
    allocated_chunks_++;
    return chunk;
}

void *ZoneSlab::AllocateFromPage(ZonePage *page, bool *full) {
    auto n = chunk_size_;

    switch (kind_) {
        case PAGE_SMALL: {
            // Small Page
            uint32_t *bitmap = page->bitmap();

            int i;
            for (i = 0; i < max_chunks_; i++) {
                if ((bitmap[i / 32] & (1 << (i % 32))) == 0) {
                    break;
                }
            }
            if (i >= max_chunks_) {
                return nullptr;
            }

            void *chunk = page->payload_offset(bitmap_bytes_ + i * n);
            bitmap[i / 32] |= (1 << (i % 32));

            if (full) {
                *full = 1;
                for (i = i + 1; i < max_chunks_; i++) {
                    if ((bitmap[i / 32] & (1 << (i % 32))) == 0) {
                        *full = 0;
                        break;
                    }
                }
            }
            return chunk;
        }

        case PAGE_NORMAL: {
            // Normal Page
            uint32_t bitmap = SLAB_BITS(page->slab());
            int i;
            for (i = 0; i < max_chunks_; i++) {
                if (((1 << i) & bitmap) == 0) {
                    break;
                }
            }
            if (i >= max_chunks_) {
                return nullptr;
            }

            void *chunk = page->payload_offset(i * n);
            bitmap |= (1 << i);
            page->set_slab(MAKE_SLAB(bitmap, SLAB_SHIFT(page->slab())));
            if (full) {
                *full = 1;
                for (i = i + 1; i < max_chunks_; i++) {
                    if ((bitmap & (1 << i)) == 0) {
                        *full = 0;
                        break;
                    }
                }
            }
            return chunk;
        }
            
        default:
            break;
    }
    
    return nullptr;
}

void ZoneSlab::FreeToPage(ZonePage *page, const void *p) {
    auto empty = true;
    auto shift = static_cast<int>(SLAB_SHIFT(page->slab()));

    switch (kind_) {

        case PAGE_SMALL: {
            uint32_t *bitmap = page->bitmap();

            auto i = reinterpret_cast<intptr_t>(
                static_cast<const uint8_t *>(p) -
                static_cast<const uint8_t *>(page->payload_offset(bitmap_bytes_)))
                    >> (shift + Zone::kMinAllocatedShift);

            DCHECK(i >= 0 && i < max_chunks_) << "free: index out of range!";
            DCHECK((bitmap[i / 32] & (1 << (i % 32))) != 0) << "bitmap duplicated zero!";

            bitmap[i / 32] &= ~(1 << (i % 32));

            // slab->bits / 4
            const int k = (int)(bitmap_bytes_ >> 2);
            for (i = 0; i < k; i++) {
                if (bitmap[i]) {
                    empty = false;
                    break;
                }
            }
        } break;

        case PAGE_NORMAL: {
            int i = (int)(((uint8_t *)p - (uint8_t *)(page + 1))
                          >> (shift + Zone::kMinAllocatedShift));
            DCHECK(i >= 0 && i < max_chunks_) << "free: index out of range!";

            uint32_t bitmap = SLAB_BITS(page->slab());
            bitmap &= ~(1 << i);
            page->set_slab(MAKE_SLAB(bitmap, shift));

            empty = (bitmap == 0);
        } break;

        default:
            break;
    }

    allocated_chunks_--;
    List::Remove(page);
    if (empty) {
        List::InsertHead(&empty_, page);
    } else {
        List::InsertHead(&partial_, page);
    }
}

ZonePage *ZoneSlab::NewPage(int shift) {
    auto *chunk = valloc(Zone::kPageSize);
    if (!chunk) {
        DLOG(ERROR) << "allocate page fail! out of memory!";
        return nullptr;
    }

    auto ptr = reinterpret_cast<intptr_t>(chunk);
    DCHECK(ptr % Zone::kPageSize == 0) << "not align to page_size";

    auto page = static_cast<ZonePage *>(chunk);
    List::Init(page);
    page->set_slab(MAKE_SLAB(0, shift));

    if (kind_ == PAGE_SMALL) {
        memset(page->bitmap(), 0, bitmap_bytes_);
    }
    return page;
}

inline void *ZoneSlab::HitCache() {
    DCHECK(List::IsNotEmpty(&cache_));

    auto header = List::Head<List::Entry>(&cache_);
    List::Remove(header);
    cached_size_ -= chunk_size_;
    return static_cast<void *>(header);
}

inline size_t ZoneSlab::HitCache(void *chunk) {
    DCHECK_NOTNULL(chunk);

    List::InsertHead(&cache_, static_cast<List::Entry *>(chunk));
    cached_size_ += chunk_size_;
    return cached_size_;
}

inline size_t ZoneSlab::PurgeCache(size_t keeped_size) {
    while (cached_size_ > keeped_size) {
        auto header = List::Head<List::Entry>(&cache_);
        List::Remove(header);

        FreeToPage(ZonePage::AlignmentGet(header),
                   ZagBytesFill(kZoneFreeBytes, header, chunk_size_));

        cached_size_ -= chunk_size_;
    }
    return cached_size_;
}

Zone::Zone()
    : slabs_(new Slab[kNumberOfSlabs]) {
    for (int i = 0; i < kNumberOfSlabs; i++) {
        slabs_[i].Init(kMinAllocatedSize << i);
    }
}

Zone::~Zone() {
    for (int i = 0; i < kNumberOfSlabs; i++) {
        slabs_[i].Clear();
    }
}

size_t Zone::slab_chunk_size(int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, kNumberOfSlabs);

    return kMinAllocatedSize << index;
}

int Zone::slab_max_chunks(int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, kNumberOfSlabs);

    return slabs_[index].max_chunks();
}

void *Zone::Allocate(size_t size) {
    if (!size) {
        return nullptr;
    }

    if (size > (kPageSize / 2)) {
        DLOG(ERROR) << "zone can not allocate memory, too large " << size;
        return nullptr;
    }

    int shift;
    for (shift = 0; (kMinAllocatedSize << shift) < size; shift++)
        ;
    DCHECK(shift >= 0 && shift < kNumberOfSlabs) << "alloc: shift out of range!";

    void *result = nullptr;
    auto slab = slabs_ + shift;
    if (slab->is_cache_not_empty()) {
        result = slab->HitCache();
    } else {
        result = slab->Allocate(shift);
    }
    return ZagBytesFill(kZoneInitialBytes, result, size);
}

void Zone::Free(const void *p) {
    if (!p) {
        return;
    }

    auto page = ZonePage::AlignmentGet(p);

    auto shift = static_cast<int>(SLAB_SHIFT(page->slab()));
    DCHECK(shift >= 0 && shift < kNumberOfSlabs) << "free: shift out of range!";

    auto slab = slabs_ + shift;
    if (slab->HitCache(const_cast<void *>(p)) > max_cache_bytes_) {
        slab->PurgeCache(keeped_cache_bytes_);
    }

}

void Zone::AssertionTest() {
    Free(CHECK_NOTNULL(Allocate(kMinAllocatedSize)));
}

void Zone::PreheatEverySlab() {
    for (int i = 0; i < kNumberOfSlabs; i++) {
        auto chunk_size = (kMinAllocatedSize << i);
        DLOG(INFO) << "perheat slab: " << chunk_size;

        Free(DCHECK_NOTNULL(Allocate(chunk_size)));
    }
}

void Zone::TEST_Report() {
    DLOG(INFO) << "---- zone report: ----";
    DLOG(INFO) << "max chace size: " << max_cache_bytes_;
    DLOG(INFO) << "keeped chace size: " << keeped_cache_bytes_;

    for (int i = 0; i < kNumberOfSlabs; ++i) {
        DLOG(INFO) << "slab[" << i << "] size: " << (kMinAllocatedSize << i);
        DLOG(INFO) << "= cache size: " << slabs_[i].cached_size();
        //DLOG(INFO) << "= total size: " << slabs_[i]
    }

    DLOG(INFO) << "---- end of zone report ----";
}

} // namespace mio
