#ifndef MIO_FILE_OUTPUT_STREAM_H_
#define MIO_FILE_OUTPUT_STREAM_H_

#include "text-output-stream.h"

namespace mio {

class FileOutputStream : public TextOutputStream {
public:
    FileOutputStream(const std::string &file_name, FILE *fp);
    FileOutputStream() = default;

    virtual ~FileOutputStream() override;
    virtual const char *file_name() const override;
    virtual std::string error() override;
    virtual int Write(const char *z, int n) override;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FileOutputStream)
private:
    std::string file_name_;
    FILE *fp_;
};

} // namespace mio

#endif // MIO_FILE_OUTPUT_STREAM_H_
