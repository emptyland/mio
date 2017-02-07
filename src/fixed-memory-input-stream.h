#ifndef MIO_FIXED_MEMORY_INPUT_STREAM_H_
#define MIO_FIXED_MEMORY_INPUT_STREAM_H_

#include "text-input-stream.h"

namespace mio {

class FixedMemoryInputStream : public TextInputStream {
public:
    FixedMemoryInputStream(const char *buf, size_t len);
    explicit FixedMemoryInputStream(const char *z);
    explicit FixedMemoryInputStream(const std::string &s);

    virtual ~FixedMemoryInputStream();

    virtual const char *file_name() const override;
    virtual bool eof() override;
    virtual int ReadOne() override;

private:
    char *buf_ = nullptr;
    size_t len_ = 0;
    int position_ = 0;
};


} // namespace mio

#endif // MIO_FIXED_MEMORY_INPUT_STREAM_H_
