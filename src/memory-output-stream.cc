#include "memory-output-stream.h"

namespace mio {

/*virtual*/ const char *MemoryOutputStream::file_name() const {
    return "[:memory:]";
}

/*virtual*/ std::string MemoryOutputStream::error() {
    return "";
}

/*virtual*/ int MemoryOutputStream::Write(const char *z, int n) {
    buf_->append(z, n);
    return n;
}

} // namespace mio
