#ifndef MIO_HANDLES_H_
#define MIO_HANDLES_H_

namespace mio {

template<class T>
class Handle {
public:
    Handle() : object_(nullptr) {}

    template<class U>
    explicit Handle(U *object) : object_(object) {}

    explicit Handle(T *object) : object_(object) {}

    template<class U>
    Handle(const Handle<U> &other) : object_(other.get()) {}

    Handle(const Handle<T> &other) : object_(other.object_) {}

    Handle(Handle &&other) : object_(other.object_) { other.object_ = nullptr; }

    ~Handle() {/*TODO*/}

    T *get() const { return object_; }

    bool empty() const { return object_ == nullptr; }

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

//    template<class U>
//    void assign(const Handle<U> &other) {
//        object_ = other.object_;
//    }

private:
    T *object_;
};


template<class T>
inline Handle<T> make_handle(T *obj) {
    return Handle<T>(obj);
}

} // namespace mio

#endif // MIO_HANDLES_H_
