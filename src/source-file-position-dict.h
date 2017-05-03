#ifndef MIO_SOURCE_FILE_POSITION_DICT_H_
#define MIO_SOURCE_FILE_POSITION_DICT_H_

#include "base.h"
#include <string>
#include <unordered_map>

namespace mio {

struct SourceFileLine {
    int         line;
    int         column;
    const char *content;
    int         content_size;
};

class TextStreamFactory;
class InternalPositionIndex;

class SourceFilePositionDict {
public:
    typedef SourceFileLine Line;

    SourceFilePositionDict();
    ~SourceFilePositionDict();

    Line GetLine(const char *file_name, int position, bool *ok);

    bool BuildIndexIfNeeded(const char *file_name);
private:
    std::unordered_map<std::string, InternalPositionIndex*> files_;
    TextStreamFactory *factory_;
}; // class SourceFilePositionDict

} // namespace mio

#endif // MIO_SOURCE_FILE_POSITION_DICT_H_
