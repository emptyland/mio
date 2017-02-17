#ifndef MIO_ZONE_VECTOR_H_
#define MIO_ZONE_VECTOR_H_

#include "zone.h"
#include "glog/logging.h"

namespace mio {

template<class T>
class ZoneVector : ManagedObject {
public:
    enum { kDefaultCapacity = 8, };

    ZoneVector(Zone *zone) : zone_(DCHECK_NOTNULL(zone)) {}
    ~ZoneVector();

    DEF_GETTER(int, size)
    DEF_GETTER(int, capacity)

    void Add(const T &element);

    const T &At(int i) const;
    T *MutableAt(int i);

    void Set(int i, const T &element);

    void Resize(int new_size);
private:
    void AdvanceIfNeeded(int new_size);

    Zone *zone_;
    T *element_   = nullptr;
    int size_     = 0;
    int capacity_ = 0;

}; // class ZoneVector<T>

template<class T>
ZoneVector<T>::~ZoneVector() {
    zone_->Free(element_);
}

template<class T>
void ZoneVector<T>::Add(const T &element) {
    AdvanceIfNeeded(size_ + 1);
    element_[size_++] = element;
}

template<class T>
const T &ZoneVector<T>::At(int i) const {
    DCHECK_GE(i, 0)     << "index out of range.";
    DCHECK_LT(i, size_) << "index out of range.";
    return element_[i];
}

template<class T>
T *ZoneVector<T>::MutableAt(int i) {
    DCHECK_GE(i, 0)     << "index out of range.";
    DCHECK_LT(i, size_) << "index out of range.";
    return &element_[i];
}

template<class T>
void ZoneVector<T>::Set(int i, const T &element) {
    DCHECK_GE(i, 0)     << "index out of range.";
    DCHECK_LT(i, size_) << "index out of range.";
    element_[i] = element;
}

template<class T>
void ZoneVector<T>::Resize(int new_size) {
    DCHECK_GE(new_size, 0);
    AdvanceIfNeeded(new_size);
    size_ = new_size;
}

template<class T>
void ZoneVector<T>::AdvanceIfNeeded(int new_size) {
    if (new_size < capacity_) {
        return;
    }

    auto new_capacity = (new_size < kDefaultCapacity)
                      ? kDefaultCapacity : new_size * 2;
    auto new_element  = static_cast<T *>(zone_->Allocate(new_capacity * sizeof(T)));
    DCHECK(new_element != nullptr) << "Zone allocate fail, too large memory.";

    memcpy(new_element, element_, size_ * sizeof(T));
    capacity_ = new_capacity;

    zone_->Free(element_);
    element_  = new_element;
}

} // namespace mio

#endif // MIO_ZONE_VECTOR_H_
