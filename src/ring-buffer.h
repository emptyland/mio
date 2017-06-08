#ifndef MIO_RING_BUFFER_H_
#define MIO_RING_BUFFER_H_

#include "base.h"
#include "glog/logging.h"
#include <stdlib.h>
#include <atomic>

namespace mio {

/**
 * in-process ring buffer
 *    i32   i32       i32     capacity-size-buffer
 * +------+------+----------+------------------  --+
 * | head | tail | capacity |                   ~  |
 * +------+------+----------+------------------  --+
 */
struct RBSlice : public mio_buf_t<uint8_t> {
    void *data;

    inline void Dispose() { if (data) ::free(data); }
};

class RingBuffer {
public:
    explicit RingBuffer(int32_t capacity) : capacity_(capacity) {}

//    int free_size() const {
//        return tail_ < head_ ? head_ - tail_ : (capacity_ - tail_) + (head_);
//    }

    void *base() { return (this + 1); }

    const void *base() const { return this + 1; }

    mio_buf_t<uint8_t> buf() {
        return {
            .z = static_cast<uint8_t *>(base()),
            .n = capacity_,
        };
    }

    void Put(const void *z, int n) {
        DCHECK_LT(n, capacity_) << "putting data too large.";
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(RingBuffer)
private:
    std::atomic<int32_t> head_;
    std::atomic<int32_t> tail_;
    int32_t capacity_;
}; // class RingBuffer

} // namespace mio

#endif // MIO_RING_BUFFER_H_
