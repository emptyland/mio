#ifndef MIO_MEMORY_OUTPUT_STREAM_H_
#define MIO_MEMORY_OUTPUT_STREAM_H_

#include "text-output-stream.h"
#include "glog/logging.h"

namespace mio {

class MemoryOutputStream : public TextOutputStream {
public:
    MemoryOutputStream(std::string *buf) : buf_(DCHECK_NOTNULL(buf)) {}

    MemoryOutputStream() = default;
    virtual ~MemoryOutputStream() override = default;

    virtual const char *file_name() const override {
        return "[:memory:]";
    }

    virtual std::string error() override {
        return "";
    }

    virtual int Write(const char *z, int n) override {
        buf_->append(z, n);
        return n;
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryOutputStream)
private:
    std::string *buf_;
};

} // namespace mio

#endif // MIO_MEMORY_OUTPUT_STREAM_H_
