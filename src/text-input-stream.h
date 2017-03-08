#ifndef MIO_TEXT_INPUT_STREAM_H_
#define MIO_TEXT_INPUT_STREAM_H_

#include "base.h"
#include <string>

namespace mio {

class TextInputStream {
public:
    TextInputStream() = default;
    virtual ~TextInputStream();

    virtual const char *file_name() const = 0;

    virtual bool eof() = 0;

    virtual std::string error() = 0;

    /**
     * Read one char from stream.
     * EOF = -1
     */
    virtual int ReadOne() = 0;

private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(TextInputStream)
};

class TextStreamFactory {
public:
    TextStreamFactory() = default;
    virtual ~TextStreamFactory();

    virtual TextInputStream *GetInputStream(const std::string &key) = 0;

private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(TextStreamFactory)
};

TextStreamFactory *CreateFileStreamFactory();

} // namespace mio

#endif // MIO_TEXT_INPUT_STREAM_H_
