#include "source-file-position-dict.h"
#include "file-input-stream.h"
#include "glog/logging.h"
#include <vector>
#include <memory>


namespace mio {

struct InternalSourceFileLine {
    int position;
    int size;
};

class InternalPositionIndex {
public:
    bool SearchPosition(int *line, int *position) const {
        for (int i = 0; i < lines_.size(); ++i) {
            if (*position >= lines_[i].position &&
                *position < lines_[i].position + lines_[i].size) {
                *line = i;
                *position -= lines_[i].position;
                return true;
            }
        }
        return false;
    }

    void AddLine(int position, int size) { lines_.push_back({position, size}); }

private:
    std::vector<InternalSourceFileLine> lines_;
};

SourceFilePositionDict::SourceFilePositionDict()
    : factory_(CreateFileStreamFactory()) {

}

SourceFilePositionDict::~SourceFilePositionDict() {
    delete factory_;
}

SourceFileLine
SourceFilePositionDict::GetLine(const char *file_name, int position, bool *ok) {
    SourceFileLine result;
    auto iter = files_.find(file_name);
    if (iter != files_.end()) {
        auto index = iter->second;
        result.column = position;
        *ok = index->SearchPosition(&result.line, &result.column);

        return result;
    } else {
        *ok = BuildIndexIfNeeded(file_name);
        if (!*ok) {
            return result;
        }
        iter = files_.find(file_name);
        auto index = iter->second;
        result.column = position;
        *ok = index->SearchPosition(&result.line, &result.column);
        
        return result;
    }
}

bool SourceFilePositionDict::BuildIndexIfNeeded(const char *file_name) {
    std::unique_ptr<TextInputStream> input(factory_->GetInputStream(file_name));
    if (!input->error().empty()) {
        DLOG(ERROR) << "open input stream fail: (" << file_name << ") " << input->error();
        return false;
    }

    std::unique_ptr<InternalPositionIndex> index(new InternalPositionIndex());
    int position = 0, line_position = 0, line_size = 0;
    while (!input->eof()) {
        if (input->ReadOne() == '\n') {
            index->AddLine(line_position, line_size + 1);
            line_size = 0;
            line_position = position + 1;
        } else {
            ++line_size;
        }
        ++position;
    }
    index->AddLine(line_position, line_size); // add last line.
    files_.emplace(file_name, index.release());
    return true;
}

} // namespace mio
