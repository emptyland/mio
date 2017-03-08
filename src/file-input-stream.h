#ifndef FILE_INPUT_STREAM_H_
#define FILE_INPUT_STREAM_H_

#include "text-input-stream.h"
#include <stdio.h>

namespace mio {

class FileInputStream : public TextInputStream {
public:
    FileInputStream(const std::string &file_name, FILE *fp);

    virtual ~FileInputStream() override;
    virtual const char *file_name() const override;
    virtual bool eof() override;
    virtual std::string error() override;
    virtual int ReadOne() override;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FileInputStream);
private:
    FILE *fp_ = nullptr;
    std::string file_name_;
}; // class FileInputStream



class FileStreamFactory : public TextStreamFactory {
public:
    FileStreamFactory();

    virtual ~FileStreamFactory() override;
    virtual TextInputStream *GetInputStream(const std::string &key) override;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FileStreamFactory)
}; // class FileStreamFactory

} // namespace mio

#endif // FILE_INPUT_STREAM_H_
