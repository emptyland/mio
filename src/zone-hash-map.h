#ifndef MIO_ZONE_HASH_MAP_H_
#define MIO_ZONE_HASH_MAP_H_

#include "zone.h"
#include "raw-string.h"
#include "glog/logging.h"
#include <string>

namespace mio {

template<class K, class V> class ZoneHashMap;
template<class K, class V> class ZoneHashMapIterator;

// The hashers
template<class T>
struct Hasher {
    static inline int Compute(const T &input) { return 0; }
    static inline bool Equal(const T &lhs, const T &rhs) { return lhs == rhs; }
}; // struct Hasher

template<>
struct Hasher<int> {
    static inline int Compute(const int &input) {
        return input;
    }
    static inline bool Equal(const int &lhs, const int &rhs) {
        return lhs == rhs;
    }
}; // struct Hasher<int>

template<>
struct Hasher<int64_t> {
    static inline int Compute(const int64_t &input) {
        return static_cast<int>(((input * input) >> 16) & 0xffffffff);
    }
    static inline bool Equal(const int64_t &lhs, const int64_t &rhs) {
        return lhs == rhs;
    }
}; // struct Hasher<int>

template<>
struct Hasher<std::string> {
    static inline int Compute(const std::string &input) {
        unsigned int hash = 1315423911;
        for (auto c : input) {
            hash ^= ((hash << 5) + c + (hash >> 2));
        }
        return (hash & 0x7FFFFFFF);
    }
    static inline bool Equal(const std::string &lhs, const std::string &rhs) {
        return lhs == rhs;
    }
}; // struct Hasher<std::string>

template<>
struct Hasher<RawStringRef> {
    static inline int Compute(RawStringRef input) {
        unsigned int hash = 1315423911;
        for (int i = 0; i < input->size(); ++i) {
            hash ^= ((hash << 5) + input->at(i) + (hash >> 2));
        }
        return (hash & 0x7FFFFFFF);
    }
    static inline bool Equal(RawStringRef lhs, RawStringRef rhs) {
        return lhs->Compare(rhs) == 0;
    }
};// struct Hasher<RawStringRef>

template<class K, class V>
class ZoneHashMapPair : public ManagedObject {
public:
    ZoneHashMapPair() {}

    DEF_GETTER(K, key)
    DEF_PROP_RMW(V, value)

    friend class ZoneHashMap<K, V>;
    friend class ZoneHashMapIterator<K, V>;
private:
    ZoneHashMapPair *next_ = nullptr;
    K key_;
    V value_;
}; // class ZoneHashMapPair<K, V>


template<class K, class V>
class ZoneHashMap : public ManagedObject {
public:
    static const int kDefaultNumberOfSlots = 16;

    ZoneHashMap(Zone *zone) : zone_(DCHECK_NOTNULL(zone)) { Init(); }
    ~ZoneHashMap();

    DEF_GETTER(int, size);
    DEF_GETTER(int, num_slots);

    bool is_empty() const { return size_ == 0; }
    bool is_not_empty() const { return !is_empty(); }

    typedef ZoneHashMapPair<K, V>     Pair;
    typedef ZoneHashMapIterator<K, V> Iterator;

    inline bool Put(const K &key, const V &value);
    Pair *GetOrInsert(const K &key, bool *has_insert);
    Pair *Get(const K &key);
    bool Exist(const K &key) { return Get(key) != nullptr; }

    friend class ZoneHashMapIterator<K, V>;
private:
    void Init();
    Pair **CreateSlots(int size);
    bool RehashIfNeed();

    static inline Pair **GetSlotByKey(const K &key, Pair **slots, int num_slots) {
        auto code = Hasher<K>::Compute(key) | 1;
        return &slots[code % num_slots];
    }

    Zone *zone_;
    Pair **slots_  = nullptr;
    int num_slots_ = 0;
    int size_      = 0;
}; // class ZoneHashMap<K, V>

// ZoneHashMap::Iterator iter(&map);
// for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
//     iter->value();
//     iter->key();
//     ...
// }
template<class K, class V>
class ZoneHashMapIterator {
public:
    typedef ZoneHashMapPair<K, V> Pair;
    typedef ZoneHashMap<K, V>     Map;

    ZoneHashMapIterator(Map *map)
        : slots_(map->slots_)
        , num_slots_(map->num_slots_) {}

    void Init() {
        for (int i = 0; i < num_slots_; ++i) {
            if (slots_[i]) {
                current_ = slots_[i];
                slot_index_ = i;
                return;
            }
        }
        slot_index_ = num_slots_;
    }

    bool HasNext() const { return slot_index_ < num_slots_; }

    void MoveNext() {
        current_ = current_->next_;
        if (!current_) {
            for (int i = slot_index_ + 1; i < num_slots_; ++i) {
                if (slots_[i]) {
                    current_ = slots_[i];
                    slot_index_ = i;
                    return;
                }
            }
            slot_index_ = num_slots_;
        }

    }

//    const K &key() const { return current_->key(); }
//    const V &value() const { return current_->value(); }
//    void set_value(const V &other) { current_->set_value(other); }
//    V *mutable_value() { current_->mutable_value(); }

    Pair *operator -> () const { return DCHECK_NOTNULL(current_); }

private:
    Pair **slots_;
    int    num_slots_;
    int    slot_index_ = 0;
    Pair  *current_ = nullptr;
};

template<class K, class V>
ZoneHashMap<K, V>::~ZoneHashMap() {
    for (int i = 0; i < num_slots_; ++i) {
        auto slot = &slots_[i];

        while (*slot) {
            Pair *header = *slot;
            *slot = header->next_;
            header->~Pair();
            zone_->Free(header);
        }
    }
    zone_->Free(static_cast<void *>(slots_));
}

template<class K, class V>
void ZoneHashMap<K, V>::Init() {
    slots_ = CreateSlots(kDefaultNumberOfSlots);
    if (slots_) {
        num_slots_ = kDefaultNumberOfSlots;
    }
}

template<class K, class V>
typename ZoneHashMap<K, V>::Pair **
ZoneHashMap<K, V>::CreateSlots(int size) {
    auto chunk = zone_->Allocate(sizeof(Pair*) * size);
    if (!chunk) {
        return nullptr;
    }
    memset(chunk, 0, sizeof(Pair*) * size);
    return static_cast<Pair **>(chunk);
}

template<class K, class V>
inline bool ZoneHashMap<K, V>::Put(const K &key, const V &value) {
    bool has_insert = false;
    auto pair = GetOrInsert(key, &has_insert);
    DCHECK_NOTNULL(pair)->value_ = value;
    return has_insert;
}

template<class K, class V>
typename ZoneHashMap<K, V>::Pair *
ZoneHashMap<K, V>::GetOrInsert(const K &key, bool *has_insert) {
    RehashIfNeed();

    auto slot = GetSlotByKey(key, slots_, num_slots_);
    for (auto iter = *slot; iter; iter = iter->next_) {
        if (Hasher<K>::Equal(iter->key_, key)) {
            if (has_insert) {
                *has_insert = false;
            }
            return iter;
        }
    }

    auto pair = new (zone_) Pair();
    pair->key_  = key;
    pair->next_ = *slot;
    *slot = pair;
    if (has_insert) {
        *has_insert = true;
    }
    ++size_;
    return pair;
}

template<class K, class V>
typename ZoneHashMap<K, V>::Pair *
ZoneHashMap<K, V>::Get(const K &key) {
    RehashIfNeed();

    auto slot = GetSlotByKey(key, slots_, num_slots_);
    for (auto iter = *slot; iter; iter = iter->next_) {
        if (Hasher<K>::Equal(iter->key_, key)) {
            return iter;
        }
    }

    return nullptr;
}

template<class K, class V>
bool ZoneHashMap<K, V>::RehashIfNeed() {
    if (size_ <= num_slots_ * 3) {
        return false;
    }

    auto new_num_slots = num_slots_ * 2;
    auto new_slots = CreateSlots(new_num_slots);
    DCHECK(new_slots != nullptr) << "zone allocation fail";

    for (int i = 0; i < num_slots_; ++i) {
        auto slot = &slots_[i];
        while (*slot) {
            auto header = *slot;
            auto new_slot = GetSlotByKey(header->key_, new_slots, new_num_slots);

            *slot = header->next_;
            header->next_ = *new_slot;
            *new_slot = header;
        }
    }

    zone_->Free(static_cast<void *>(slots_));
    slots_     = new_slots;
    num_slots_ = new_num_slots;
    return true;
}

} // namespace mio

#endif // MIO_ZONE_HASH_MAP_H_
