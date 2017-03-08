#include "file-input-stream.h"
#include "glog/logging.h"

namespace mio {

namespace {

class ErrorInputStream : public TextInputStream {
public:
    ErrorInputStream(const std::string &file_name, const std::string &message)
        : file_name_(file_name)
        , message_(message) {}

    virtual ~ErrorInputStream() {}

    virtual const char *file_name() const override { return "[:error:]"; }
    virtual bool eof() override { return true; }
    virtual std::string error() override { return message_; }
    virtual int ReadOne() override { return -1; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(ErrorInputStream);
private:
    std::string file_name_;
    std::string message_;
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
//// FileInputStream
////////////////////////////////////////////////////////////////////////////////

FileInputStream::FileInputStream(const std::string &file_name, FILE *fp)
    : file_name_(file_name)
    , fp_(DCHECK_NOTNULL(fp)) {}

/*virtual*/ FileInputStream::~FileInputStream() {
    if (fp_) {
        fclose(fp_);
    }
}

/*virtual*/ const char *FileInputStream::file_name() const {
    return file_name_.c_str();
}

/*virtual*/ bool FileInputStream::eof() {
    return feof(fp_);
}

/*virtual*/ std::string FileInputStream::error() {
    char buf[256];
    strerror_r(ferror(fp_), buf, arraysize(buf));
    return std::string(buf);
}

/*virtual*/ int FileInputStream::ReadOne() {
    return fgetc(fp_);
}

////////////////////////////////////////////////////////////////////////////////
//// FileStreamFactory
////////////////////////////////////////////////////////////////////////////////

FileStreamFactory::FileStreamFactory() {
}

/*virtual*/ FileStreamFactory::~FileStreamFactory() {
}

/*virtual*/
TextInputStream *FileStreamFactory::GetInputStream(const std::string &key) {
    auto fp = fopen(key.c_str(), "r");
    if (!fp) {
        char buf[256];
        strerror_r(errno, buf, arraysize(buf));
        return new ErrorInputStream(key, buf);
    }

    return new FileInputStream(key, fp);
}

TextStreamFactory *CreateFileStreamFactory() {
    return new FileStreamFactory();
}

} // namespace mio
