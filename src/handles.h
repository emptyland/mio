#ifndef MIO_HANDLES_H_
#define MIO_HANDLES_H_

namespace mio {

template<class T>
class Local {
public:
    Local() : object_(nullptr) {}
    explicit Local(T *object) : object_(object) {}
    Local(const Local &other) : object_(other.object_) {}
    Local(Local &&other) : object_(other.object_) { other.object_ = nullptr; }

    ~Local() {/*TODO*/}

    T *get() const { return object_; }

    bool empty() const { return object_ == nullptr; }

    T *operator -> () const { return get(); }

    void operator = (T *object) { object_ = object; }

    void operator = (const Local<T> &other) { object_ = other.object_; }

    void operator = (Local<T> &&other) {
        object_ = other.object_;
        other.object_ = nullptr;
    }

private:
    T *object_;
};

template<class T>
inline Local<T> make_local(T *obj) {
    return Local<T>(obj);
}

} // namespace mio

#endif // MIO_HANDLES_H_
