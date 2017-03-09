#include "simple-file-system.h"
#include <sys/stat.h>
#include <dirent.h>

namespace mio {

namespace posix {

class PlatformSimpleFileSystem : public SimpleFileSystem {
public:
    PlatformSimpleFileSystem() = default;
    virtual ~PlatformSimpleFileSystem() override = default;

    virtual bool IsDir(const char *path) override {
        struct stat s;
        if (stat(path, &s) < 0) {
            return false;
        }
        return S_ISDIR(s.st_mode);
    }

    virtual int GetNames(const char *dir, const char *ext,
                         std::vector<std::string> *names) override {
        auto dirp = opendir(dir);
        if (!dirp) {
            return -1;
        }

        auto ext_len = ext ? strlen(ext) : 0;
        struct dirent *dp = nullptr;
        while ((dp = readdir(dirp)) != nullptr) {
            if (dp->d_name[0] == '.') {
                continue;
            }

            if (ext) {
                auto found = strnstr(dp->d_name, ext, dp->d_namlen);
                if (found == dp->d_name + dp->d_namlen - ext_len) {
                    names->push_back(dp->d_name);
                }
            } else {
                names->push_back(dp->d_name);
            }
        }
        return static_cast<int>(names->size());
    }
};

} // namespace posix

SimpleFileSystem *CreatePlatformSimpleFileSystem() {
    return new posix::PlatformSimpleFileSystem();
}

} // namespace mio
