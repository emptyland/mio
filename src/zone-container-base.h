#ifndef ZONE_CONTAINER_BASE_H_
#define ZONE_CONTAINER_BASE_H_

#include "zone.h"
#include "base.h"
#include "glog/logging.h"

namespace mio {

template<class T>
class ZoneLinkedArray {
public:
    struct Segment {
        Segment *next;
        T        data[0];
    };

    enum { kDefaultCapacity = 8, };

    explicit ZoneLinkedArray(Zone *zone)
        : zone_(DCHECK_NOTNULL(zone))
        , segment_max_capacity_(CalcSegmentMaxCapacity(zone)) {
        DCHECK_GT(segment_max_capacity_, kDefaultCapacity);
        head_ = NewSegment(capacity_);
        tail_ = head_;
    }

    DEF_GETTER(int, size)
    DEF_GETTER(int, capacity)
    DEF_GETTER(int, segment_max_capacity)

    int GetTailSize() const {
        return (GetSegmentSize() == GetUsedSegmentSize()) ?
               size_ % segment_max_capacity_ : 0;
    }

    int GetTailCapacity() const { return capacity_ % segment_max_capacity_; }


    int GetSegmentSize() const {
        return (capacity_ + segment_max_capacity_ - 1) / segment_max_capacity_;
    }

    int GetUsedSegmentSize() const {
        return (size_ + segment_max_capacity_ - 1) / segment_max_capacity_;
    }

    inline void Add(const T &element) {
        IncreaseIfNeeded();
        auto i = size_ / segment_max_capacity_;
        auto p = head_;
        while (i--) {
            p = p->next;
        }
        DCHECK_NOTNULL(p)->data[size_++ % segment_max_capacity_] = element;
    }

    inline void Set(int index, const T &element) {
        DCHECK(index >= 0 && index < size_);
        auto i = index / segment_max_capacity_;
        auto p = head_;
        while (i--) {
            p = p->next;
        }
        DCHECK_NOTNULL(p)->data[index % segment_max_capacity_] = element;
    }

    inline T Get(int index) const {
        DCHECK(index >= 0 && index < size_);
        auto i = index / segment_max_capacity_;
        auto p = head_;
        while (i--) {
            p = p->next;
        }
        return DCHECK_NOTNULL(p)->data[index % segment_max_capacity_];
    }

    void IncreaseIfNeeded() {
        if (size_ + 1 <= capacity_) {
            return;
        }

        auto tail_capacity = GetTailCapacity();
        auto new_capacity  = capacity_ * 2;
        auto incresement_capacity = new_capacity - capacity_;

        Segment stub, *prev = &stub;
        stub.next = head_;
        while (prev->next != tail_) {
            prev = prev->next;
        }
        if (tail_capacity > 0) {
            auto segment_new_capacity = new_capacity <= segment_max_capacity_ ?
                                        new_capacity : segment_max_capacity_;
            auto tail = NewSegment(segment_new_capacity);
            incresement_capacity -= (segment_new_capacity - tail_capacity);
            memcpy(tail->data, prev->next->data, sizeof(T) * GetTailSize());
            zone_->Free(prev->next);
            prev->next = tail;
            tail_ = tail;
        }
        head_ = stub.next;

        for (int i = 0; i < incresement_capacity / segment_max_capacity_; ++i) {
            auto tail = NewSegment(segment_max_capacity_);
            tail_->next = tail;
            tail_ = tail;
        }

        if (incresement_capacity % segment_max_capacity_ != 0) {
            auto tail = NewSegment(new_capacity % segment_max_capacity_);
            tail_->next = tail;
            tail_ = tail;
        }
        capacity_ = new_capacity;
    }

private:
    inline Segment *NewSegment(int capacity) {
        auto seg = static_cast<Segment *>(zone_->Allocate(sizeof(Segment) + sizeof(T) * capacity));
        seg->next = nullptr;
        return seg;
    }

    inline int CalcSegmentMaxCapacity(Zone *zone) {
        int chunk_size = 0;
        auto max_capacity  = zone->GetMaxChunks(sizeof(T), &chunk_size);
        max_capacity -= (sizeof(Segment) + (chunk_size - 1)) / chunk_size;
        return max_capacity;
    }

    Segment  *head_ = nullptr;
    Segment  *tail_ = nullptr;
    Zone     *zone_;
    int       size_ = 0;
    int       capacity_ = kDefaultCapacity;
    const int segment_max_capacity_;
};

} // namespace mio

#endif // ZONE_CONTAINER_BASE_H_
