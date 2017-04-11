#ifndef MIO_VM_OBJECTS_H_
#define MIO_VM_OBJECTS_H_

#include "handles.h"
#include "base.h"
#include "glog/logging.h"

namespace mio {

class VM;
class Thread;

class HeapObject;
class MIOString;
class MIOError;
class MIOFunction;
    class MIONativeFunction;
    class MIONormalFunction;
class MIOUpValue;
class MIOClosure;
class MIOUnion;
class MIOHashMap;
class MIOReflectionType;
    class MIOReflectionVoid;
    class MIOReflectionIntegral;
    class MIOReflectionFloating;
    class MIOReflectionString;
    class MIOReflectionError;
    class MIOReflectionUnion;
    class MIOReflectionMap;
    class MIOReflectionFunction;

#define MIO_REFLECTION_TYPES(M) \
    M(ReflectionVoid) \
    M(ReflectionIntegral) \
    M(ReflectionFloating) \
    M(ReflectionString) \
    M(ReflectionError) \
    M(ReflectionUnion) \
    M(ReflectionMap) \
    M(ReflectionFunction)

#define MIO_OBJECTS(M) \
    M(String) \
    M(UpValue) \
    M(Closure) \
    M(NativeFunction) \
    M(NormalFunction) \
    M(HashMap) \
    M(Error) \
    M(Union) \
    MIO_REFLECTION_TYPES(M)

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

    bool IsReflectionType() const {
        switch (GetKind()) {
    #define HeapObject_REFLECTION_TYPE(name) \
        case k##name: return true;
            MIO_REFLECTION_TYPES(HeapObject_REFLECTION_TYPE)
    #undef  HeapObject_REFLECTION_TYPE
            default:
                break;
        }
        return false;
    }

    MIOReflectionType *AsReflectionType() {
        return IsReflectionType() ? reinterpret_cast<MIOReflectionType *>(this) : nullptr;
    }

    const MIOReflectionType *AsReflectionType() const {
        return IsReflectionType() ? reinterpret_cast<const MIOReflectionType *>(this) : nullptr;
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(HeapObject)
}; // class HeapObject

union InternalAllValue {
    HeapObject *object;
    mio_f64_t   f64;
    mio_i64_t   i64;
};

static const int kMaxReferenceValueSize = sizeof (InternalAllValue);

class MIOString : public HeapObject {
public:
    typedef mio_strbuf_t Buffer;

    static const int kLengthOffset = kHeapObjectOffset;
    static const int kDataOffset = kLengthOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(int, Length)

    const char *GetData() const {
        return reinterpret_cast<const char *>(this) + kDataOffset;
    }

    Buffer Get() const {
        return { .z = GetData(), .n = GetLength(), };
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
    static const int kPrimitiveArgumentsSizeOffset = kSignatureOffset + sizeof(int);
    static const int kObjectArgumentsSizeOffset = kPrimitiveArgumentsSizeOffset + sizeof(int);
    static const int kNativePointerOffset = kObjectArgumentsSizeOffset + sizeof(HeapObject *);
    static const int kMIONativeFunctionOffset = kNativePointerOffset + sizeof(MIOFunctionPrototype);

    DEFINE_HEAP_OBJ_RW(MIOString *, Signature)
    DEFINE_HEAP_OBJ_RW(int, PrimitiveArgumentsSize)
    DEFINE_HEAP_OBJ_RW(int, ObjectArgumentsSize)
    DEFINE_HEAP_OBJ_RW(MIOFunctionPrototype, NativePointer)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIONativeFunction)
}; // class MIONativeFunction

static_assert(sizeof(MIONativeFunction) == sizeof(HeapObject),
              "MIONativeFunction can bigger than HeapObject");

class MIONormalFunction final: public MIOFunction {
public:
    static const int kConstantObjectSizeOffset = kMIOFunctionOffset;
    static const int kConstantPrimitiveSizeOffset = kConstantObjectSizeOffset + sizeof(int);
    static const int kCodeSizeOffset = kConstantPrimitiveSizeOffset + sizeof(int);
    static const int kHeaderOffset = kCodeSizeOffset;

    DEFINE_HEAP_OBJ_RW(int, ConstantObjectSize)
    DEFINE_HEAP_OBJ_RW(int, ConstantPrimitiveSize)
    DEFINE_HEAP_OBJ_RW(int, CodeSize)

    HeapObject **GetConstantObjects() {
        return reinterpret_cast<HeapObject **>(
                reinterpret_cast<uint8_t *>(this) + GetConstantPrimitiveSize() + kHeaderOffset);
    }

    HeapObject *GetConstantObject(int index) {
        DCHECK_GE(index, 0);
        DCHECK_LT(index, GetConstantObjectSize());
        return GetConstantObjects()[index];
    }

    void *GetConstantPrimitive() {
        return reinterpret_cast<uint8_t *>(this) + kHeaderOffset;
    }

    void *GetCode() {
        return reinterpret_cast<uint8_t *>(this) + kHeaderOffset +
                (sizeof(HeapObject *) * GetConstantPrimitiveSize()) + GetConstantObjectSize();
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIONormalFunction)
}; // class NormalFunction

static_assert(sizeof(MIONormalFunction) == sizeof(HeapObject),
              "MIONormalFunction can bigger than HeapObject");


class MIOUpValue final : public HeapObject {
public:

}; // class MIOUpValue;

union UpValDesc {
    MIOUpValue *val;
    struct {
        int32_t unique_id;
        int32_t offset;
    } desc;
};

class MIOClosure final : public MIOFunction {
public:
    static const int kFlagsOffset = kMIOFunctionOffset;
    static const int kFunctionOffset = kFlagsOffset + sizeof(uint32_t);
    static const int kUpValueSizeOffset = kFunctionOffset + kObjectReferenceSize;
    static const int kUpValesOffset = kUpValueSizeOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(uint32_t, Flags)
    DEFINE_HEAP_OBJ_RW(MIOFunction *, Function)
    DEFINE_HEAP_OBJ_RW(int, UpValueSize)

    bool IsOpen() const  { return (GetFlags() & 0x1) == 0; }
    bool IsClose() const { return (GetFlags() & 0x1) != 0; }
    void Close() { SetFlags(GetFlags() | 0x1);     }

    UpValDesc **mutable_up_values() {
        return reinterpret_cast<UpValDesc **>(reinterpret_cast<uint8_t *>(this) + kUpValesOffset);
    }

    UpValDesc *GetUpValue(int index) {
        DCHECK_GE(index, 0);
        DCHECK_LT(index, GetUpValueSize());
        return mutable_up_values()[index];
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOClosure)
}; // class MIOClosure

static_assert(sizeof(MIOClosure) == sizeof(HeapObject),
              "MIOClosure can bigger than HeapObject");

class MIOUnion final : public HeapObject {
public:
    static const int kTypeInfoOffset = kHeapObjectOffset;
    static const int kDataOffset     = kTypeInfoOffset + kObjectReferenceSize;
    static const int kMIOUnionOffset = kDataOffset   + kMaxReferenceValueSize;

    DEFINE_HEAP_OBJ_RW(MIOReflectionType *, TypeInfo)

    void *mutable_data() {
        return reinterpret_cast<uint8_t *>(this) + kDataOffset;
    }

    const void *data() const {
        return reinterpret_cast<const uint8_t *>(this) + kDataOffset;
    }

    template<class T>
    inline T GetData() const { return *static_cast<const T *>(data()); }

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

////////////////////////////////////////////////////////////////////////////////
/// Reflection Objects
////////////////////////////////////////////////////////////////////////////////
class MIOReflectionType : public HeapObject {
public:
    static const int kTidOffset = kHeapObjectOffset;
    static const int kReferencedSizeOffset = kTidOffset + sizeof(int64_t);
    static const int kMIOReflectionTypeOffset = kReferencedSizeOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(int64_t, Tid)
    DEFINE_HEAP_OBJ_RW(int, ReferencedSize)

    bool IsPrimitive() const { return IsReflectionIntegral() || IsReflectionFloating(); }
    bool IsVoid() const { return IsReflectionVoid(); }
    bool IsObject() const { return !IsPrimitive() && !IsVoid(); }

    int GetPlacementSize() const;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionType)
}; // class MIOReflectionType

class MIOReflectionVoid final : public MIOReflectionType {
public:
    static const int kMIOReflectionVoidOffset = kMIOReflectionTypeOffset;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionVoid)
}; // class MIOReflectionVoid

class MIOReflectionIntegral final : public MIOReflectionType {
public:
    static const int kBitWideOffset = kMIOReflectionTypeOffset;
    static const int kMIOReflectionIntegralOffset = kBitWideOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(int, BitWide)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionIntegral)
}; // class MIOReflectionIntegral

class MIOReflectionFloating final : public MIOReflectionType {
public:
    static const int kBitWideOffset = kMIOReflectionTypeOffset;
    static const int kMIOReflectionFloatingOffset = kBitWideOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(int, BitWide)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionFloating)
}; // class MIOReflectionFloating

class MIOReflectionString final : public MIOReflectionType {
public:
    static const int kMIOReflectionStringOffset = kMIOReflectionTypeOffset;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionString)
}; // class MIOReflectionString

class MIOReflectionError final : public MIOReflectionType {
public:
    static const int kMIOReflectionErrorOffset = kMIOReflectionTypeOffset;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionError)
}; // class MIOReflectionError

class MIOReflectionUnion final : public MIOReflectionType {
public:
    static const int kMIOReflectionUnionOffset = kMIOReflectionTypeOffset;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionUnion)
}; // class MIOReflectionUnion

class MIOReflectionMap final : public MIOReflectionType {
public:
    static const int kKeyOffset = kMIOReflectionTypeOffset;
    static const int kValueOffset = kKeyOffset + sizeof(MIOReflectionType *);
    static const int kMIOReflectionMapOffset = kValueOffset + sizeof(MIOReflectionType *);

    DEFINE_HEAP_OBJ_RW(MIOReflectionType *, Key)
    DEFINE_HEAP_OBJ_RW(MIOReflectionType *, Value)

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionMap)
}; // class MIOReflectionMap

class MIOReflectionFunction final : public MIOReflectionType {
public:
    static const int kReturnOffset = kMIOReflectionTypeOffset;
    static const int kNumberOfParametersOffset = kReturnOffset + sizeof(MIOReflectionType *);
    static const int kParamtersOffset = kNumberOfParametersOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(MIOReflectionType *, Return)
    DEFINE_HEAP_OBJ_RW(int, NumberOfParameters)

    MIOReflectionType **GetParamters() {
        auto base = reinterpret_cast<uint8_t *>(this) + kParamtersOffset;
        return reinterpret_cast<MIOReflectionType **>(base);
    }

    inline MIOReflectionType *GetParamter(int index) {
        DCHECK_GE(index, 0);
        DCHECK_LT(index, GetNumberOfParameters());
        return GetParamters()[index];
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionFunction)
}; // class MIOReflectionFunction

} // namespace mio

#endif // MIO_VM_OBJECTS_H_
