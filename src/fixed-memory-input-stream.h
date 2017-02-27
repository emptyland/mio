#ifndef MIO_FIXED_MEMORY_INPUT_STREAM_H_
#define MIO_FIXED_MEMORY_INPUT_STREAM_H_

#include "text-input-stream.h"
#include <unordered_map>

namespace mio {

class FixedMemoryInputStream : public TextInputStream {
public:
    FixedMemoryInputStream(const char *buf, size_t len);
    explicit FixedMemoryInputStream(const char *z);
    explicit FixedMemoryInputStream(const std::string &s);

    virtual ~FixedMemoryInputStream();

    virtual const char *file_name() const override;
    virtual bool eof() override;
    virtual std::string error() override;
    virtual int ReadOne() override;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FixedMemoryInputStream);
private:
    char *buf_ = nullptr;
    size_t len_ = 0;
    int position_ = 0;
}; // class FixedMemoryInputStream


class FixedMemoryStreamFactory : public TextStreamFactory {
public:
    FixedMemoryStreamFactory();
    virtual ~FixedMemoryStreamFactory();

    void PutInputStream(const std::string &name, const std::string content);

    virtual TextInputStream *GetInputStream(const std::string &key) override;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FixedMemoryStreamFactory)
private:
    std::unordered_map<std::string, TextInputStream *> input_streams_;
}; // class FixedMemoryStreamFactory

} // namespace mio

#endif // MIO_FIXED_MEMORY_INPUT_STREAM_H_
