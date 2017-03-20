#ifndef MIO_VM_OBJECTS_H_
#define MIO_VM_OBJECTS_H_

#include "base.h"

namespace mio {

class VM;
class Thread;

class HeapObject;
class MIOString;
class MIOFunction;
    class MIONativeFunction;
    class MIONormalFunction;

#define MIO_OBJECTS(M) \
    M(String) \
    M(NativeFunction) \
    M(NormalFunction)

typedef int (*MIOFunctionPrototype)(VM *, Thread *);

template<class T>
inline T HeapObjectGet(const HeapObject *obj, int offset) {
    return *reinterpret_cast<const T *>(reinterpret_cast<const uint8_t *>(obj) + offset);
}

template<class T>
inline void HeapObjectSet(HeapObject *obj, int offset, T value) {
    *reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(obj) + offset) = value;
}

#define DEFINE_HEAP_OBJ_GETTER(type, name) \
    type Get##name() const { return HeapObjectGet<type>(this, k##name##Offset); }

#define DEFINE_HEAP_OBJ_SETTER(type, name) \
    void Set##name(type value) { HeapObjectSet<type>(this, k##name##Offset, value); }

#define DEFINE_HEAP_OBJ_RW(type, name) \
    DEFINE_HEAP_OBJ_GETTER(type, name) \
    DEFINE_HEAP_OBJ_SETTER(type, name)

class HeapObject {
public:
    enum Kind: int {
    #define HeapObject_ENUM_KIND(name) k##name,
        MIO_OBJECTS(HeapObject_ENUM_KIND)
    #undef  HeapObject_ENUM_KIND
    };

    static const int kKindOffset = 0;
    static const int kHeapObjectOffset = kKindOffset + 4;

    DEFINE_HEAP_OBJ_RW(Kind, Kind)

#define HeapObject_TYPE_CAST(name) \
    bool Is##name() const { return GetKind() == k##name; } \
    MIO##name *As##name() { \
        return Is##name() ? reinterpret_cast<MIO##name *>(this) : nullptr; \
    } \
    const MIO##name *As##name() const { \
        return Is##name() ? reinterpret_cast<const MIO##name *>(this) : nullptr; \
    }
    MIO_OBJECTS(HeapObject_TYPE_CAST)
#undef HeapObject_TYPE_CAST

    DISALLOW_IMPLICIT_CONSTRUCTORS(HeapObject)
}; // class HeapObject


class MIOString : public HeapObject {
public:
    static const int kLengthOffset = kHeapObjectOffset;
    static const int kDataOffset = kLengthOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(int, Length)

    const char *GetData() const {
        return reinterpret_cast<const char *>(this) + kDataOffset;
    }

    char *mutable_data() {
        return reinterpret_cast<char *>(this) + kDataOffset;
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOString)
}; // class MIOString

class MIOFunction : public HeapObject {
public:
    enum Type: int {
        NATIVE,
        NORMAL,
    };

    static const int kTypeOffset = kHeapObjectOffset;
    static const int kMIOFunctionOffset = kHeapObjectOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(Type, Type)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOFunction)
}; // class FunctionObject

class MIONativeFunction : public MIOFunction {
public:

    static const int kSignatureOffset = kMIOFunctionOffset;
    static const int kNativePointerOffset = kMIOFunctionOffset + sizeof(HeapObject *);
    static const int kMIONativeFunctionOffset = kNativePointerOffset + sizeof(MIOFunctionPrototype);

    DEFINE_HEAP_OBJ_RW(MIOString *, Signature)
    DEFINE_HEAP_OBJ_RW(MIOFunctionPrototype, NativePointer)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIONativeFunction)
}; // class MIONativeFunction

class MIONormalFunction : public MIOFunction {
public:

    static const int kAddressOffset = kMIOFunctionOffset;
    static const int kMIONormalFunctionOffset = kAddressOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(int, Address);

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIONormalFunction)
}; // class NormalFunction

} // namespace mio

#endif // MIO_VM_OBJECTS_H_
