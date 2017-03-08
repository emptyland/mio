#ifndef MIO_SIMPLE_FILE_SYSTEM_H_
#define MIO_SIMPLE_FILE_SYSTEM_H_

#include "base.h"
#include <vector>
#include <string>

namespace mio {

class SimpleFileSystem {
public:
    SimpleFileSystem() = default;
    virtual ~SimpleFileSystem() = default;

    virtual bool IsDir(const char *path) = 0;

    virtual int GetNames(const char *dir, const char *ext,
                         std::vector<std::string> *names) = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(SimpleFileSystem)
};

SimpleFileSystem *CreatePlatformSimpleFileSystem();

} // namespace mio

#endif // MIO_SIMPLE_FILE_SYSTEM_H_
