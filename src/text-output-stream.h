#ifndef TEXT_OUTPUT_STREAM_H_
#define TEXT_OUTPUT_STREAM_H_

#include "raw-string.h"
#include "base.h"
#include <stdarg.h>
#include <string>

namespace mio {

class TextOutputStream {
public:
    TextOutputStream() = default;
    virtual ~TextOutputStream() = default;

    virtual const char *file_name() const = 0;

    virtual std::string error() = 0;

    virtual int Write(const char *z, int n) = 0;

    int Write(RawStringRef raw) { return Write(raw->c_str(), raw->size()); }
    int Write(const char *z) { return Write(z, static_cast<int>(strlen(z))); }

    __attribute__ (( __format__ (__printf__, 2, 3)))
    int Printf(const char *fmt, ...);
    int Vprintf(const char *fmt, va_list ap);

    __attribute__ (( __format__ (__printf__, 1, 2)))
    static std::string sprintf(const char *fmt, ...);
    static std::string vsprintf(const char *fmt, va_list ap);


    DISALLOW_IMPLICIT_CONSTRUCTORS(TextOutputStream)
};

} // namespace mio

#endif // TEXT_OUTPUT_STREAM_H_
