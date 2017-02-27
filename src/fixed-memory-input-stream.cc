#include "fixed-memory-input-stream.h"

namespace mio {

namespace {

class ErrorInputStream : public TextInputStream {
public:
    explicit ErrorInputStream(const std::string &message) : message_(message) {}
    virtual ~ErrorInputStream() {}

    virtual const char *file_name() const override { return "[:error:]"; }
    virtual bool eof() override { return true; }
    virtual std::string error() override { return message_; }
    virtual int ReadOne() override { return -1; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(ErrorInputStream);
private:
    std::string message_;
};

} // namespace

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

/*virtual*/ FixedMemoryInputStream::~FixedMemoryInputStream() {
    delete[] buf_;
}

/*virtual*/ const char *FixedMemoryInputStream::file_name() const  {
    return "[:memory:]";
}

/*virtual*/ bool FixedMemoryInputStream::eof() {
    return position_ >= len_;
}

/*virtual*/ std::string FixedMemoryInputStream::error() {
    return std::string();
}

/*virtual*/ int FixedMemoryInputStream::ReadOne() {
    if (position_ >= len_) {
        return -1;
    } else {
        return buf_[position_++];
    }
}

FixedMemoryStreamFactory::FixedMemoryStreamFactory() {
}

/*virtual*/ FixedMemoryStreamFactory::~FixedMemoryStreamFactory() {
    for (const auto &pair : input_streams_) {
        delete pair.second;
    }
}

void FixedMemoryStreamFactory::PutInputStream(const std::string &name,
                                              const std::string content) {
    auto stream = new FixedMemoryInputStream(content);
    input_streams_.emplace(name, stream);
}

/*virtual*/
TextInputStream *
FixedMemoryStreamFactory::GetInputStream(const std::string &key) {
    auto iter = input_streams_.find(key);
    if (iter == input_streams_.end()) {
        return new ErrorInputStream("key not found: " + key);
    }
    auto stream = iter->second;
    input_streams_.erase(iter);
    return stream;
}

} // namespace mio
