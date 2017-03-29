#ifndef MIO_HANDLES_H_
#define MIO_HANDLES_H_

namespace mio {

template<class T>
class Local {
public:
    explicit Local(T *object) : object_(object) {}
    Local(const Local &other) : object_(other.object_) {}
    Local(Local &&other) : object_(other.object_) { other.object_ = nullptr; }

    ~Local() {/*TODO*/}

    T *get() const { return object_; }

    bool empty() const { return object_ == nullptr; }

    T *operator -> () const { return get(); }

private:
    T *object_;
};

template<class T>
inline Local<T> make_local(T *obj) {
    return Local<T>(obj);
}

} // namespace mio

#endif // MIO_HANDLES_H_
