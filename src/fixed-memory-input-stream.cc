#include "fixed-memory-input-stream.h"

namespace mio {

FixedMemoryInputStream::FixedMemoryInputStream(const char *buf, size_t len) {
    buf_ = new char[len];
    memcpy(buf_, buf, len);
    len_ = len;
}

FixedMemoryInputStream::FixedMemoryInputStream(const char *z)
    : FixedMemoryInputStream(z, strlen(z)) {
}

FixedMemoryInputStream::FixedMemoryInputStream(const std::string &s)
    : FixedMemoryInputStream(s.data(), s.size()) {
}

/* virtual */ FixedMemoryInputStream::~FixedMemoryInputStream() {
    delete[] buf_;
}

/* virtual */ const char *FixedMemoryInputStream::file_name() const  {
    return "[:memory:]";
}

/* virtual */ bool FixedMemoryInputStream::eof() {
    return position_ >= len_;
}

/* virtual */ int FixedMemoryInputStream::ReadOne() {
    if (position_ >= len_) {
        return -1;
    } else {
        return buf_[position_++];
    }
}

} // namespace mio
