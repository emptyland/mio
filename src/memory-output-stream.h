#ifndef MIO_MEMORY_OUTPUT_STREAM_H_
#define MIO_MEMORY_OUTPUT_STREAM_H_

#include "text-output-stream.h"
#include "glog/logging.h"
#include <string>

namespace mio {

class MemoryOutputStream : public TextOutputStream {
public:
    MemoryOutputStream(std::string *buf) : buf_(DCHECK_NOTNULL(buf)) {}
    MemoryOutputStream() = default;

    virtual ~MemoryOutputStream() override = default;
    virtual const char *file_name() const override;
    virtual std::string error() override;
    virtual int Write(const char *z, int n) override;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryOutputStream)
private:
    std::string *buf_;
};

} // namespace mio

#endif // MIO_MEMORY_OUTPUT_STREAM_H_
