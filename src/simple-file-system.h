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

    virtual bool Exist(const char *path) = 0;

    virtual bool Mkdir(const char *path, bool recursive) = 0;

    virtual int GetNames(const char *dir, const char *ext,
                         std::vector<std::string> *names) = 0;

    inline std::string Search(const char *name,
                              const std::vector<std::string> &search_path);

    DISALLOW_IMPLICIT_CONSTRUCTORS(SimpleFileSystem)
};

SimpleFileSystem *CreatePlatformSimpleFileSystem();

inline
std::string
SimpleFileSystem::Search(const char *name,
                         const std::vector<std::string> &search_path) {
    for (const auto &sp : search_path) {
        std::string path(sp);
        path.append("/");
        path.append(name);

        if (Exist(path.c_str())) {
            return path;
        }
    }

    return std::string();
}

} // namespace mio

#endif // MIO_SIMPLE_FILE_SYSTEM_H_
