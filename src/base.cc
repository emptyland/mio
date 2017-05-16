#include "base.h"
#include "glog/logging.h"
#include <type_traits>
#include <unistd.h>

namespace mio {

int kPageSizeShift = 12;

int kPageSize = (1 << kPageSizeShift); // 4 KB

uintptr_t kPageAlignmentMask = -1;

int kNumberOfCpuCores = 1;

template<class T>
inline void *RoundBytesFill(const T zag, void *chunk, size_t n) {
    static_assert(sizeof(zag) > 1 && sizeof(zag) <= 8, "T must be int16,32,64");
    static_assert(std::is_integral<T>::value, "T must be int16,32,64");

    DCHECK_GE(n, 0);
    auto result = chunk;
    auto round = n / sizeof(T);
    while (round--) {
        auto round_bits = static_cast<T *>(chunk);
        *round_bits = zag;
        chunk = static_cast<void *>(round_bits + 1);
    }

    auto zag_bytes = reinterpret_cast<const uint8_t *>(&zag);

    round = n % sizeof(T);
    while (round--) {
        auto bits8 = static_cast<uint8_t *>(chunk);
        *bits8 = *zag_bytes++;
        chunk = static_cast<void *>(bits8 + 1);
    }
    return result;
}

void *Round16BytesFill(const uint16_t zag, void *chunk, size_t n) {
    return RoundBytesFill<uint16_t>(zag, chunk, n);
}

void *Round32BytesFill(const uint32_t zag, void *chunk, size_t n) {
    return RoundBytesFill<uint32_t>(zag, chunk, n);
}

void *Round64BytesFill(const uint64_t zag, void *chunk, size_t n) {
    return RoundBytesFill<uint64_t>(zag, chunk, n);
}

void EnvirmentInitialize() {
    kPageSize = static_cast<int>(sysconf(_SC_PAGESIZE));
    PLOG_IF(FATAL, kPageSize <= 0) << "can not get page size!";

    int shift;
    for (shift = 0; (1 << shift) < kPageSize; shift++)
        ;
    kPageSizeShift = shift;
    kPageAlignmentMask = ~(kPageSize - 1);
    DLOG(INFO) << "page size: " << kPageSize << " shift: " << kPageSizeShift;

    kNumberOfCpuCores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    DLOG(INFO) << "number of cpu cores: " << kNumberOfCpuCores;
}

} // namespace mio
