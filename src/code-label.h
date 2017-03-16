#ifndef MIO_CODE_LABEL_H_
#define MIO_CODE_LABEL_H_

#include "base.h"
#include "glog/logging.h"

namespace mio {

class CodeLabel {
public:
    CodeLabel() = default;

    inline int position() const;
    bool is_bound() const { return pos_ < 0; }
    bool is_unused() const { return pos_ == 0 && near_link_pos_ == 0; }
    bool is_linked() const { return pos_ > 0; }
    bool is_near_linked() const { return near_link_pos_ > 0; }

    int near_position() const { return near_link_pos_ - 1; }

    inline void BindTo(int for_bind);
    inline void LinkTo(int for_link, bool is_far);

    DISALLOW_IMPLICIT_CONSTRUCTORS(CodeLabel)
private:
    int pos_ = 0;
    int near_link_pos_ = 0;
}; // class CodeLabel


inline int CodeLabel::position() const {
    if (pos_ < 0)
        return -pos_ - 1;
    if (pos_ > 0)
        return pos_ - 1;
    DLOG(FATAL) << "noreached! pos_ == 0";
    return 0;
}

inline void CodeLabel::BindTo(int for_bind) {
    pos_ = -for_bind - 1;
    DCHECK(is_bound());
}

inline void CodeLabel::LinkTo(int for_link, bool is_far) {
    if (!is_far) {
        near_link_pos_ = (for_link) + 1;
        DCHECK(is_near_linked());
    } else {
        pos_ = (for_link) + 1;
        DCHECK(is_linked());
    }
}

} // namespace mio

#endif // MIO_CODE_LABEL_H_
