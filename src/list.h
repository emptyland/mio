#ifndef MIO_LIST_MACROS_H_
#define MIO_LIST_MACROS_H_

namespace mio {

class List {
public:
    struct Entry {
        Entry *next;
        Entry *prev;
    };

    inline static void Init(Entry *h) {
        (h)->next = (h);
        (h)->prev = (h);
    }

    template<class T>
    inline static T *As(Entry *h) {
        return reinterpret_cast<T *>(h);
    }

    template<class T>
    inline static T *Head(Entry *h) { return As<T>(h->next); }

    template<class T>
    inline static T *Tail(Entry *h) { return As<T>(h->prev); }

    inline static void InsertHead(Entry *h, Entry *x) {
        (x)->next = (h)->next;
        (x)->next->prev = x;
        (x)->prev = h;
        (h)->next = x;
    }

    inline static void InsertTail(Entry *h, Entry *x) {
        (x)->prev = (h)->prev;
        (x)->prev->next = x;
        (x)->next = h;
        (h)->prev = x;
    }

    inline static void Remove(Entry *x) {
        (x)->next->prev = (x)->prev;
        (x)->prev->next = (x)->next;
    }

    inline static void Concat(Entry *h, Entry *n) {
        (h)->prev->next = (n)->next;
        (n)->next->prev = (h)->prev;
        (h)->prev = (n)->prev;
        (h)->prev->next = (h);
    }

    inline static bool IsEmpty(Entry *h) { return h->next == h; }
    inline static bool IsNotEmpty(Entry *h) { return !IsEmpty(h); }

};

// macros from nginx/src/core/queue.h


} // namespace mio

#endif // MIO_LIST_MACROS_H_
