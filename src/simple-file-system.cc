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

    virtual bool Exist(const char *path) override {
        struct stat s;
        return stat(path, &s) == 0;
    }

    virtual bool Mkdir(const char *path, bool recursive) override {
        if (!recursive) {
            return ::mkdir(path, 0755) == 0;
        }

        std::string all(path);
        std::string::size_type pos = 0, begin = 0;
        do {
            pos = all.find('/', begin);
            auto partial = all.substr(0, pos);
            begin = pos + 1;

            if (Exist(partial.c_str())) {
                continue;
            }
            if (::mkdir(partial.c_str(), 0755) != 0) {
                return false;
            }
        } while (pos != std::string::npos);
        return true;
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
