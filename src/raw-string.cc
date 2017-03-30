#include "raw-string.h"
#include "text-output-stream.h"

namespace mio {

static const char kMainValueBlob[] = "\x04\x00\x00\x00main";
const RawString *const kMainValue = reinterpret_cast<RawStringRef>(kMainValueBlob);

//static const char kBootstrapBlob[] = "\x09\x00\x00\x00bootstrap";
//const RawString *const kBootsrapValue = reinterpret_cast<RawStringRef>(kBootstrapBlob);

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

/*static*/ RawStringRef RawString::sprintf(Zone *zone, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto rv = RawString::vsprintf(zone, fmt, ap);
    va_end(ap);
    return rv;
}

/*static*/ RawStringRef RawString::vsprintf(Zone *zone, const char *fmt,
                                            va_list ap) {
    return Create(TextOutputStream::vsprintf(fmt, ap), zone);
}

} // namespace mio
