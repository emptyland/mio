#include "raw-string.h"

namespace mio {

static const int kEmptyStringBlob[2] = { 0, 0 };

static_assert(sizeof(kEmptyStringBlob) >= sizeof(RawString),
              "RawString too small.");

const RawString *const RawString::kEmpty = reinterpret_cast<RawStringRef>(kEmptyStringBlob);

RawStringRef RawString::Create(const char *z, size_t n, Zone *zone) {
    if (!z || *z == '\0' || n == 0) {
        return kEmpty;
    }

    auto placement_size = sizeof(RawString) + n + 1;
    auto chunk = zone->Allocate(placement_size);
    if (!chunk) {
        return nullptr;
    }
    auto result = new (chunk) RawString(static_cast<int>(n));
    memcpy(result + 1, z, n);

    // fill tail zero
    reinterpret_cast<char *>(result + 1)[n] = '\0';
    return result;
}

} // namespace mio
