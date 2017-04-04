#ifndef MIO_VM_OBJECTS_H_
#define MIO_VM_OBJECTS_H_

#include "handles.h"
#include "base.h"

namespace mio {

class VM;
class Thread;

class HeapObject;
class MIOString;
class MIOError;
class MIOFunction;
    class MIONativeFunction;
    class MIONormalFunction;
class MIOUnion;
class MIOHashMap;

#define MIO_OBJECTS(M) \
    M(String) \
    M(NativeFunction) \
    M(NormalFunction) \
    M(HashMap) \
    M(Error)

typedef int (*MIOFunctionPrototype)(VM *, Thread *);

template<class T>
inline T HeapObjectGet(const void *obj, int offset) {
    return *reinterpret_cast<const T *>(reinterpret_cast<const uint8_t *>(obj) + offset);
}

template<class T>
inline void HeapObjectSet(void *obj, int offset, T value) {
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

static_assert(sizeof(MIOString) == sizeof(HeapObject),
              "MIOString can bigger than HeapObject");

class MIOFunction : public HeapObject {
public:
    static const int kNameOffset = kHeapObjectOffset;
    static const int kMIOFunctionOffset = kHeapObjectOffset + kObjectReferenceSize;

    DEFINE_HEAP_OBJ_RW(MIOString *, Name)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOFunction)
}; // class FunctionObject

static_assert(sizeof(MIOFunction) == sizeof(HeapObject),
              "MIOFunction can bigger than HeapObject");

class MIONativeFunction : public MIOFunction {
public:
    static const int kSignatureOffset = kMIOFunctionOffset;
    static const int kNativePointerOffset = kMIOFunctionOffset + sizeof(HeapObject *);
    static const int kMIONativeFunctionOffset = kNativePointerOffset + sizeof(MIOFunctionPrototype);

    DEFINE_HEAP_OBJ_RW(MIOString *, Signature)
    DEFINE_HEAP_OBJ_RW(MIOFunctionPrototype, NativePointer)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIONativeFunction)
}; // class MIONativeFunction

static_assert(sizeof(MIONativeFunction) == sizeof(HeapObject),
              "MIONativeFunction can bigger than HeapObject");

class MIONormalFunction final: public MIOFunction {
public:
    static const int kCodeSizeOffset = kMIOFunctionOffset;
    static const int kCodeOffset = kCodeSizeOffset + sizeof(int);
    static const int kHeadOffset = kCodeOffset;

    DEFINE_HEAP_OBJ_RW(int, CodeSize)

    void *GetCode() { return reinterpret_cast<uint8_t *>(this) + kCodeOffset; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIONormalFunction)
}; // class NormalFunction

static_assert(sizeof(MIONormalFunction) == sizeof(HeapObject),
              "MIONormalFunction can bigger than HeapObject");

class MIOUnion : public HeapObject {
public:
    static const int kTypeIdOffset = kHeapObjectOffset;
    static const int kDataOffset   = kTypeIdOffset + sizeof(int64_t);
    static const int kMIOUnionOffset = kDataOffset + kObjectReferenceSize;

    DEFINE_HEAP_OBJ_RW(int64_t, TypeId)

    void *data() { return reinterpret_cast<uint8_t *>(this) + kDataOffset; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOUnion)
}; // class MIOUnion

static_assert(sizeof(MIOUnion) == sizeof(HeapObject),
              "MIOUnion can bigger than HeapObject");


class MapPair;

class MIOHashMap : public HeapObject {
public:
    static const int kDefaultInitialSlots = 4;

    static const int kSeedOffset = kHeapObjectOffset;
    static const int kFlagsOffset = kSeedOffset + sizeof(int);
    static const int kSizeOffset = kFlagsOffset + sizeof(uint32_t);
    static const int kNumberOfSlotsOffset = kSizeOffset + sizeof(int);
    static const int kSlotsOffset = kNumberOfSlotsOffset + sizeof(int);
    static const int kMIOHashMapOffset = kSlotsOffset + sizeof(MapPair *);

    DEFINE_HEAP_OBJ_RW(int, Seed)
    DEFINE_HEAP_OBJ_RW(uint32_t, Flags)
    DEFINE_HEAP_OBJ_RW(int, Size)
    DEFINE_HEAP_OBJ_RW(int, NumberOfSlots)
    DEFINE_HEAP_OBJ_RW(MapPair *, Slots)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOHashMap)
}; // class MIOHashMap

static_assert(sizeof(MIOHashMap) == sizeof(HeapObject),
              "MIOHashMap can bigger than HeapObject");

class MIOPair {
public:
    static const int kNextOffset = 0;
    static const int kPrevOffset = kNextOffset + sizeof(MapPair *);
    static const int kPayloadOffset = kPrevOffset + sizeof(MapPair *);
    static const int kKeyOffset = kPayloadOffset;
    static const int kValueOffset = kKeyOffset + kObjectReferenceSize;
    static const int kMIOMapPairOffset = kValueOffset + kObjectReferenceSize;

    DEFINE_HEAP_OBJ_RW(MapPair *, Next)
    DEFINE_HEAP_OBJ_RW(MapPair *, Prev)

    template<class T>
    inline T GetKey() { return HeapObjectGet<T>(this, kKeyOffset); }

    template<class T>
    inline T GetValue() { return HeapObjectGet<T>(this, kValueOffset); }

    template<class T>
    inline void SetValue(T value) {
        return HeapObjectSet<T>(this, kValueOffset, value);
    }
}; // class MIOMapPair


class MIOError : public HeapObject {
public:
    static const int kLinkedErrorOffset = kHeapObjectOffset;
    static const int kMessageOffset     = kLinkedErrorOffset + kObjectReferenceSize;
    static const int kPositionOffset    = kMessageOffset + sizeof(int);
    static const int kMIOErrorOffset    = kPositionOffset + kObjectReferenceSize;

    DEFINE_HEAP_OBJ_RW(MIOError *, LinkedError)
    DEFINE_HEAP_OBJ_RW(int, Position)
    DEFINE_HEAP_OBJ_RW(MIOString *, Message)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOError)
}; // class MIOError

static_assert(sizeof(MIOError) == sizeof(HeapObject),
              "MIOError can bigger than HeapObject");

} // namespace mio

#endif // MIO_VM_OBJECTS_H_
