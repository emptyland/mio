#ifndef MIO_OBJECT_TRAITS_H_
#define MIO_OBJECT_TRAITS_H_

#include "vm-objects.h"

namespace mio {

////////////////////////////////////////////////////////////////////////////////
/// NativeValue analyer
////////////////////////////////////////////////////////////////////////////////

template<class T>
struct NativeValue {};

template<>
struct NativeValue<mio_i8_t> {
    inline bool Check(char s) const { return s == '8'; }

    inline const char *type() const { return "mio_i8_t"; }

    inline const void *Address(mio_i8_t *value) {
        return static_cast<const void *>(value);
    }

    inline mio_i8_t Deref(void *addr) {
        return *static_cast<mio_i8_t *>(addr);
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionIntegral() &&
        type->AsReflectionIntegral()->GetBitWide() == 8;
    }
};

template<>
struct NativeValue<mio_i16_t> {
    inline bool Check(char s) const { return s == '7'; }

    inline const char *type() const { return "mio_i16_t"; }

    inline const void *Address(mio_i16_t *value) {
        return static_cast<const void *>(value);
    }

    inline mio_i16_t Deref(void *addr) {
        return *static_cast<mio_i16_t *>(addr);
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionIntegral() &&
        type->AsReflectionIntegral()->GetBitWide() == 16;
    }
};

template<>
struct NativeValue<mio_i32_t> {
    inline bool Check(char s) const { return s == '5'; }

    inline const char *type() const { return "mio_i32_t"; }

    inline const void *Address(mio_i32_t *value) {
        return static_cast<const void *>(value);
    }

    inline mio_i32_t Deref(void *addr) {
        return *static_cast<mio_i32_t *>(addr);
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionIntegral() &&
        type->AsReflectionIntegral()->GetBitWide() == 32;
    }
};

template<>
struct NativeValue<mio_i64_t> {
    inline bool Check(char s) const { return s == '9'; }

    inline const char *type() const { return "mio_int_t/mio_i64_t"; }

    inline const void *Address(mio_i64_t *value) {
        return static_cast<const void *>(value);
    }

    inline mio_i64_t Deref(void *addr) {
        return *static_cast<mio_i64_t *>(addr);
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionIntegral() &&
        type->AsReflectionIntegral()->GetBitWide() == 64;
    }
};

template<>
struct NativeValue<mio_f32_t> {
    inline bool Check(char s) const { return s == '3'; }

    inline const char *type() const { return "mio_f32_t"; }

    inline const void *Address(mio_f32_t *value) {
        return static_cast<const void *>(value);
    }

    inline mio_f32_t Deref(void *addr) {
        return *static_cast<mio_f32_t *>(addr);
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionFloating() &&
        type->AsReflectionIntegral()->GetBitWide() == 32;
    }
};

template<>
struct NativeValue<mio_f64_t> {
    inline bool Check(char s) const { return s == '6'; }

    inline const char *type() const { return "mio_f64_t"; }

    inline const void *Address(mio_f64_t *value) {
        return static_cast<const void *>(value);
    }

    inline mio_f64_t Deref(void *addr) {
        return *static_cast<mio_f64_t *>(addr);
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionFloating() &&
        type->AsReflectionIntegral()->GetBitWide() == 64;
    }
};

template<>
struct NativeValue<void> {
    inline bool Check(char s) const { return s == '!'; }
    inline const char *type() const { return "void"; }
};

template<>
struct NativeValue<HeapObject*> {
    inline bool Check(char s) const {
        return s == 's' || s == 'a' || s == 'm' || s == 'r' || s == 'u' ||
        s == 'z' || s == 'e';
    }
    inline const char *type() const { return "HeapObject *"; }
};

template<>
struct NativeValue<MIOString *> {
    inline bool Check(char s) const { return s == 'z'; }
    inline const char *type() const { return "MIOString *"; }
};

template<>
struct NativeValue<MIOError *> {
    inline bool Check(char s) const { return s == 'e'; }
    inline const char *type() const { return "MIOError *"; }
};

template<>
struct NativeValue<MIOUnion *> {
    inline bool Check(char s) const { return s == 'u'; }
    inline const char *type() const { return "MIOUnion *"; }
};

template<>
struct NativeValue<MIOExternal *> {
    inline bool Check(char s) const { return s == 'x'; }
    inline const char *type() const { return "MIOExternal *"; }
};

template<>
struct NativeValue<MIOSlice *> {
    inline bool Check(char s) const { return s == 's'; }
    inline const char *type() const { return "MIOSlice *"; }
};

template<>
struct NativeValue<MIOVector *> {
    inline bool Check(char s) const { return s == 'a'; }
    inline const char *type() const { return "MIOVector *"; }
};

template<>
struct NativeValue<MIOHashMap *> {
    inline bool Check(char s) const { return s == 'm'; }
    inline const char *type() const { return "MIOHashMap *"; }
};

template<>
struct NativeValue<MIOFunction *> {
    inline bool Check(char s) const { return s == 'r'; }
    inline const char *type() const { return "MIOFunction *"; }
};

template<>
struct NativeValue<Handle<MIOString>> {
    inline const void *Address(Handle<MIOString> *value) {
        return static_cast<const void *>(value->address());
    }

    inline Handle<MIOString> Deref(void *addr) {
        return make_handle(*static_cast<MIOString **>(addr));
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionString();
    }
};

template<>
struct NativeValue<Handle<MIOError>> {
    inline const void *Address(Handle<MIOError> *value) {
        return static_cast<const void *>(value->address());
    }

    inline Handle<MIOError> Deref(void *addr) {
        return make_handle(*static_cast<MIOError **>(addr));
    }

    inline bool Allow(MIOReflectionType *type) {
        return type->IsReflectionError();
    }
};

} // namespace mio

#endif // MIO_OBJECT_TRAITS_H_
