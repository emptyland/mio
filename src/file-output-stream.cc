#include "file-output-stream.h"
#include "glog/logging.h"

namespace mio {

namespace {

class ErrorOutputStream : public TextOutputStream {
public:
    ErrorOutputStream(const std::string &file_name,
                      const std::string &message)
        : file_name_(file_name)
        , message_(message) {}
    ErrorOutputStream() = default;

    virtual ~ErrorOutputStream() override = default;
    virtual const char *file_name() const override {
        return file_name_.c_str();
    }
    virtual std::string error() override {
        return message_;
    }
    virtual int Write(const char */*z*/, int /*n*/) override { return -1; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(ErrorOutputStream)
private:
    std::string file_name_;
    std::string message_;
};

} // namespace

FileOutputStream::FileOutputStream(const std::string &file_name, FILE *fp)
    : file_name_(file_name)
    , fp_(DCHECK_NOTNULL(fp)) {}

/*virtual*/ FileOutputStream::~FileOutputStream() {
    if (fp_) {
        fclose(fp_);
    }
}

/*virtual*/ const char *FileOutputStream::file_name() const {
    return file_name_.c_str();
}

/*virtual*/ std::string FileOutputStream::error() {
    char buf[256];
    strerror_r(ferror(fp_), buf, arraysize(buf));
    return std::string(buf);
}

/*virtual*/ int FileOutputStream::Write(const char *z, int n) {
    return static_cast<int>(fwrite(z, n, 1, fp_));
}

TextOutputStream *CreateFileOutputStream(const char *file_name) {
    auto fp = fopen(file_name, "w");
    if (!fp) {
        char buf[256];
        strerror_r(errno, buf, arraysize(buf));
        return new ErrorOutputStream(file_name, buf);
    }

    return new FileOutputStream(file_name, fp);
}

} // namespace mio
