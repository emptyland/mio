#ifndef MIO_HANDLES_H_
#define MIO_HANDLES_H_

#include <atomic>

namespace mio {

template<class T>
class Handle {
public:
    Handle() : object_(nullptr) {}

    template<class U>
    explicit Handle(U *object) : object_(object) {
        if (object_) {
            object_->Grab();
        }
    }

    explicit Handle(T *object) : object_(object) {
        if (object_) {
            object_->Grab();
        }
    }

    template<class U>
    Handle(const Handle<U> &other) : object_(other.get()) {
        if (object_) {
            object_->Grab();
        }
    }

    Handle(const Handle<T> &other) : object_(other.object_) {
        if (object_) {
            object_->Grab();
        }
    }

    Handle(Handle &&other) : object_(other.object_) {
        other.object_ = nullptr;
        if (object_) {
            object_->Grab();
        }
    }

    ~Handle() {
        if (object_) {
            object_->Drop();
        }
    }

    T *get() const { return object_; }

    bool empty() const { return object_ == nullptr; }

    bool valid() const { return !empty(); }

    T *operator -> () const { return get(); }

    template<class U>
    void operator = (U *object) { object_ = object; }

    void operator = (T *object) { object_ = object; }

    template<class U>
    void operator = (const Handle<U> &other) { object_ = other.get(); }

    void operator = (const Handle<T> &other) { object_ = other.object_; }

    void operator = (Handle<T> &&other) {
        object_ = other.object_;
        other.object_ = nullptr;
    }

    T **address() { return &object_; }

private:
    T *object_;
}; // template<class T> class Handle


template <class T>
class AtomicHandle {
public:
    AtomicHandle() : object_(nullptr) {}

    template<class U>
    explicit AtomicHandle(U *object) : AtomicHandle(static_cast<T *>(object)) {}

    template<class U>
    AtomicHandle(const AtomicHandle<U> &other) : AtomicHandle(static_cast<T *>(other.get())) {}

    AtomicHandle(const AtomicHandle<T> &other) : AtomicHandle(other.object_) {}

    template<class U>
    AtomicHandle(const Handle<U> &other) : AtomicHandle(static_cast<T *>(other.get())) {}

    AtomicHandle(const Handle<T> &other) : AtomicHandle(other.get()) {}

    explicit AtomicHandle(T *object) : object_(object) {
        auto ob = object_.load(std::memory_order_relaxed);
        if (ob) {
            ob->Grab();
        }
    }

    ~AtomicHandle() {
        auto ob = object_.load(std::memory_order_relaxed);
        if (ob) {
            ob->Drop();
        }
    }

    T *get() const { return object_.load(std::memory_order_release); }

    bool empty() const { return get() == nullptr; }

    bool valid() const { return !empty(); }

    T *operator -> () const { return get(); }

    void set(T *object) { object_.store(object, std::memory_order_acquire); }

    template<class U>
    void operator = (U *object) { set(static_cast<T *>(object)); }

    void operator = (T *object) { set(object); }

    template<class U>
    void operator = (const AtomicHandle<U> &other) { set(static_cast<T *>(other.get())); }

    void operator = (const AtomicHandle<T> &other) { set(other.get()); }

    template<class U>
    void operator = (const Handle<U> &other) { set(static_cast<T *>(other.get())); }

    void operator = (const Handle<T> &other) { set(other.get()); }


    Handle<T> ToHandle() const { return Handle<T>(get()); }

private:
    std::atomic<T *> object_;
}; // template<class T> class AtomicHandle



template<class T>
inline Handle<T> make_handle(T *ob) {
    return Handle<T>(ob);
}

template<class T>
inline AtomicHandle<T> make_atomic_handle(T *ob) {
    return AtomicHandle<T>(ob);
}

} // namespace mio

#endif // MIO_HANDLES_H_
