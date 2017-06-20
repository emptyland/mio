#ifndef MIO_ZONE_VECTOR_H_
#define MIO_ZONE_VECTOR_H_

#include "zone.h"
#include "glog/logging.h"

namespace mio {

template<class T>
class ZoneVector : public ManagedObject {
public:
    enum { kDefaultCapacity = 8, };

    ZoneVector(Zone *zone) : zone_(DCHECK_NOTNULL(zone)) {}
    ~ZoneVector();

    DEF_GETTER(int, size)
    DEF_GETTER(int, capacity)

    bool is_empty() const { return size() == 0; }
    bool is_not_empty() const { return !is_empty(); }

    void Add(const T &element);
    void Assign(ZoneVector<T> &&other);

    const T &At(int i) const;
    T *MutableAt(int i);

    const T &first() const { return At(0); }
    const T &last() const { return At(size() - 1); }

    void Set(int i, const T &element);

    void Resize(int new_size);
    void Clear();
private:
    void AdvanceIfNeeded(int new_size);

    Zone *zone_;
    T *element_   = nullptr;
    int size_     = 0;
    int capacity_ = 0;

}; // class ZoneVector<T>

#define DEF_ZONE_VECTOR_PROP_RO(type, name) \
    DEF_ZONE_VECTOR_GETTER(type, name) \
    DEF_ZONE_VECTOR_SIZE(name)

#define DEF_ZONE_VECTOR_PROP_RW(type, name) \
    DEF_ZONE_VECTOR_PROP_RO(type, name) \
    DEF_ZONE_VECTOR_SETTER(type, name)

#define DEF_ZONE_VECTOR_PROP_RWA(type, name) \
    DEF_ZONE_VECTOR_PROP_RW(type, name) \
    DEF_ZONE_VECTOR_ADD(type, name)

#define DEF_ZONE_VECTOR_PROP_RMW(type, name) \
    DEF_ZONE_VECTOR_PROP_RO(type, name) \
    DEF_ZONE_VECTOR_SETTER(type, name) \
    DEF_ZONE_VECTOR_MUTABLE_GETTER(type, name)

#define DEF_PTR_ZONE_VECTOR_PROP_RO(type, name) \
    DEF_PTR_ZONE_VECTOR_GETTER(type, name) \
    DEF_PTR_ZONE_VECTOR_CONST_GETTER(type, name) \
    DEF_PTR_ZONE_VECTOR_SIZE(name)

#define DEF_PTR_ZONE_VECTOR_PROP_RW(type, name) \
    DEF_PTR_ZONE_VECTOR_PROP_RO(type, name) \
    DEF_PTR_ZONE_VECTOR_SETTER(type, name)

#define DEF_PTR_ZONE_VECTOR_PROP_RWA(type, name) \
    DEF_PTR_ZONE_VECTOR_PROP_RW(type, name) \
    DEF_PTR_ZONE_VECTOR_ADD(type, name)

#define DEF_ZONE_VECTOR_GETTER(type, name) \
    inline type name(int i) const { return name##s_.At(i); }

#define DEF_ZONE_VECTOR_SETTER(type, name) \
    inline void set_##name(int i, type value) { name##s_.Set(i, value); }

#define DEF_ZONE_VECTOR_SIZE(name) \
    inline int name##_size() const { return name##s_.size(); }

#define DEF_ZONE_VECTOR_ADD(type, name) \
    inline void add_##name(type value) { name##s_.Add(value); }

#define DEF_ZONE_VECTOR_MUTABLE_GETTER(type, name) \
    inline ZoneVector<type> *mutable_##name##s() { return &name##s_; }

#define DEF_PTR_ZONE_VECTOR_GETTER(type, name) \
    inline type name(int i) const { return name##s_->At(i); }

#define DEF_PTR_ZONE_VECTOR_SETTER(type, name) \
    inline void set_##name(int i, type value) { name##s_->Set(i, value); }

#define DEF_PTR_ZONE_VECTOR_SIZE(name) \
    inline int name##_size() const { return name##s_->size(); }

#define DEF_PTR_ZONE_VECTOR_ADD(type, name) \
    inline void add_##name(type value) { name##s_->Add(value); }

#define DEF_PTR_ZONE_VECTOR_MUTABLE_GETTER(type, name) \
    inline ZoneVector<type> *mutable_##name##s() { return name##s_; }

#define DEF_PTR_ZONE_VECTOR_CONST_GETTER(type, name) \
    inline const ZoneVector<type> *name##s() const { return name##s_; }

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
void ZoneVector<T>::Clear() {
    zone_->Free(element_);
    element_  = nullptr;
    size_     = 0;
    capacity_ = 0;
}

template<class T>
void ZoneVector<T>::Assign(ZoneVector<T> &&other) {
    element_  = other.element_;
    size_     = other.size_;
    capacity_ = other.capacity_;

    other.element_  = nullptr;
    other.size_     = 0;
    other.capacity_ = 0;
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
