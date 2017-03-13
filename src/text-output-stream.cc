#include "text-output-stream.h"

namespace mio {

int TextOutputStream::Printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto rv = Vprintf(fmt, ap);
    va_end(ap);
    return rv;
}

int TextOutputStream::Vprintf(const char *fmt, va_list ap) {
    auto rv = TextOutputStream::vsprintf(fmt, ap);
    return Write(rv.data(), static_cast<int>(rv.size()));
}

/*static*/ std::string TextOutputStream::sprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto rv = TextOutputStream::vsprintf(fmt, ap);
    va_end(ap);
    return rv;
}

/*static*/ std::string TextOutputStream::vsprintf(const char *fmt, va_list ap) {
    va_list copied;
    int len = 512, rv = len;
    std::string buf;
    do {
        len = rv + 512;
        buf.resize(len, 0);
        va_copy(copied, ap);
        rv = vsnprintf(&buf[0], len, fmt, ap);
        va_copy(ap, copied);
    } while (rv > len);
    buf.resize(strlen(buf.c_str()), 0);
    return buf;
}

} // namespace mio
