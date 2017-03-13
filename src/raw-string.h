#ifndef MIO_RAW_STRING_H_
#define MIO_RAW_STRING_H_

#include "zone.h"
#include "base.h"
#include <stdarg.h>
#include <string>

namespace mio {

class RawString;

typedef const RawString * RawStringRef;

extern const RawString *const kMainValue;


class RawString {
public:
    DEF_GETTER(int, size)

    const char *c_str() const {
        return reinterpret_cast<const char *>(this + 1);
    }

    char at(int i) const { return c_str()[i]; }

    size_t placement_size() const {
        return sizeof(*this) + size_ + 1;
    }

    std::string ToString() const {
        return std::string(c_str(), size());
    }

    int Compare(RawStringRef rhs) const {
        return this == rhs ? 0 : strcmp(c_str(), rhs->c_str());
    }

    static RawStringRef Create(const std::string &s, Zone *zone) {
        return Create(s.data(), s.size(), zone);
    }
    static RawStringRef Create(const char *z, Zone *zone) {
        return Create(z, strlen(z), zone);
    }
    static RawStringRef Create(const char *z, size_t n, Zone *zone);

    __attribute__ (( __format__ (__printf__, 2, 3)))
    static RawStringRef sprintf(Zone *zone, const char *fmt, ...);

    static RawStringRef vsprintf(Zone *zone, const char *fmt, va_list ap);

    static const RawString *const kEmpty;
private:
    RawString(int size) : size_(size) {}

    int size_;
};



} // namespace mio

#endif // MIO_RAW_STRING_H_
