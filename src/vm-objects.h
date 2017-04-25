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

#define DECLARE_VM_OBJECT(name)  \
    DECLARE_VM_OBJECT_KIND(name) \
    DECLARE_VM_OBJECT_SIZE(name)

#define DECLARE_VM_OBJECT_KIND(name) \
    enum { kSelfKind = HeapObject::k##name, };

#define DECLARE_VM_OBJECT_SIZE(name) \
    inline constexpr int GetPlacementSize() const { return kMIO##name##Offset; }

class HeapObject {
public:
    enum Kind: int {
    #define HeapObject_ENUM_KIND(name) k##name,
        MIO_OBJECTS(HeapObject_ENUM_KIND)
    #undef  HeapObject_ENUM_KIND
        MAX_KINDS,
    };

    enum Flags: uint32_t {
        GC_HANDLE_COUNT_MASK = 0x0000ffff,
        GC_COLOR_MASK        = 0x000f0000,
        GC_GENERATION_MASK   = 0x00f00000,
        KIND_MASK            = 0xff000000,
    };

    static const int kMaxGCGeneration = 0xf;
    static const int kMaxGCColor      = 0xf;

    static const int kNextOffset = 0;                                  // for double-linked list
    static const int kPrevOffset = kNextOffset + sizeof(HeapObject *); // for double-linked list
    static const int kListEntryOffset = kPrevOffset;

    // HI-8  bits: heap object of kind
    // LO-24 bits: GC flags
    static const int kHeaderFlagsOffset = kPrevOffset + sizeof(HeapObject *);
    static const int kHeapObjectOffset = kHeaderFlagsOffset + sizeof(uint32_t);

    HeapObject *InitEntry() {
        SetNext(this);
        SetPrev(this);
        return this;
    }

    HeapObject *Init(Kind kind) {
        InitEntry();
        SetHeaderFlags(0);
        SetKind(kind);
        return this;
    }

    DEFINE_HEAP_OBJ_RW(HeapObject *, Next)
    DEFINE_HEAP_OBJ_RW(HeapObject *, Prev)

    bool IsGrabbed() const { return GetHandleCount() > 0; }

    int GetHandleCount() const { return (GetHeaderFlags() & GC_HANDLE_COUNT_MASK); }

    void Grab() {
        SetHeaderFlags((GetHeaderFlags() & ~GC_HANDLE_COUNT_MASK) |
                       ((GetHandleCount() + 1) & GC_HANDLE_COUNT_MASK));
    }

    void Drop() {
        SetHeaderFlags((GetHeaderFlags() & ~GC_HANDLE_COUNT_MASK) |
                       ((GetHandleCount() - 1) & GC_HANDLE_COUNT_MASK));
    }

    int GetGeneration() const {
        return static_cast<int>((GetHeaderFlags() >> 20) & 0xf);
    }

    void SetGeneration(int g) {
        SetHeaderFlags((GetHeaderFlags() & ~GC_GENERATION_MASK) | ((g << 20) & GC_GENERATION_MASK));
    }

    int GetColor() const {
        return static_cast<int>((GetHeaderFlags() >> 16) & 0xf);
    }

    void SetColor(int c) {
        SetHeaderFlags((GetHeaderFlags() & ~GC_COLOR_MASK) | ((c << 16) & GC_COLOR_MASK));
    }

    Kind GetKind() const {
        return static_cast<Kind>((GetHeaderFlags() >> 24) & 0xff);
    }

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

    bool IsCallable() const {
        return IsNativeFunction() || IsNormalFunction() || IsClosure();
    }

    MIOFunction *AsCallable() {
        return IsCallable() ? reinterpret_cast<MIOFunction *>(this) : nullptr;
    }

    const MIOFunction *AsCallable() const {
        return IsCallable() ? reinterpret_cast<const MIOFunction *>(this) : nullptr;
    }

    int GetSize() const;

    DISALLOW_IMPLICIT_CONSTRUCTORS(HeapObject)

private:
    DEFINE_HEAP_OBJ_RW(uint32_t, HeaderFlags)

    void SetKind(Kind kind) {
        SetHeaderFlags((GetHeaderFlags() & ~KIND_MASK) | ((static_cast<int>(kind) << 24) & KIND_MASK));
    }
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
    static const int kHeaderOffset = kDataOffset;

    DEFINE_HEAP_OBJ_RW(int, Length)

    const char *GetData() const {
        return reinterpret_cast<const char *>(this) + kDataOffset;
    }

    char *GetMutableData() {
        return reinterpret_cast<char *>(this) + kDataOffset;
    }

    Buffer Get() const {
        return { .z = GetData(), .n = GetLength(), };
    }

    inline int GetPlacementSize() const { return kHeaderOffset + GetLength(); }

    bool IsUnique() const { return GetLength() <= kMaxUniqueStringSize; }

    static const MIOString *OffsetOfData(const char *data) {
        return reinterpret_cast<const MIOString *>(DCHECK_NOTNULL(data) - kDataOffset);
    }

    DECLARE_VM_OBJECT_KIND(String)
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
    static const int kPrimitiveArgumentsSizeOffset = kSignatureOffset + kObjectReferenceSize;
    static const int kObjectArgumentsSizeOffset = kPrimitiveArgumentsSizeOffset + sizeof(int);
    static const int kNativePointerOffset = kObjectArgumentsSizeOffset + sizeof(int);
    static const int kMIONativeFunctionOffset = kNativePointerOffset + sizeof(MIOFunctionPrototype);

    DEFINE_HEAP_OBJ_RW(MIOString *, Signature)
    DEFINE_HEAP_OBJ_RW(int, PrimitiveArgumentsSize)
    DEFINE_HEAP_OBJ_RW(int, ObjectArgumentsSize)
    DEFINE_HEAP_OBJ_RW(MIOFunctionPrototype, NativePointer)

    DECLARE_VM_OBJECT(NativeFunction)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIONativeFunction)
}; // class MIONativeFunction

static_assert(sizeof(MIONativeFunction) == sizeof(HeapObject),
              "MIONativeFunction can bigger than HeapObject");

class MIONormalFunction final: public MIOFunction {
public:
    static const int kConstantPrimitiveSizeOffset = kMIOFunctionOffset;
    static const int kConstantObjectSizeOffset = kConstantPrimitiveSizeOffset + sizeof(int);
    static const int kCodeSizeOffset = kConstantObjectSizeOffset + sizeof(int);
    static const int kHeaderOffset = kCodeSizeOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(int, ConstantPrimitiveSize)
    DEFINE_HEAP_OBJ_RW(int, ConstantObjectSize)
    DEFINE_HEAP_OBJ_RW(int, CodeSize)

    HeapObject **GetConstantObjects() {
        return reinterpret_cast<HeapObject **>(
                reinterpret_cast<uint8_t *>(this) + kHeaderOffset + GetConstantPrimitiveSize());
    }

    HeapObject *GetConstantObject(int index) {
        DCHECK_GE(index, 0);
        DCHECK_LT(index, GetConstantObjectSize());
        return GetConstantObjects()[index];
    }

    mio_buf_t<HeapObject *> GetConstantObjectBuf() {
        return {
            .z = GetConstantObjects(),
            .n = GetConstantObjectSize(),
        };
    }

    void *GetConstantPrimitiveData() {
        return reinterpret_cast<uint8_t *>(this) + kHeaderOffset;
    }

    mio_buf_t<uint8_t> GetConstantPrimitiveBuf() {
        return {
            .z = static_cast<uint8_t *>(GetConstantPrimitiveData()),
            .n = GetConstantPrimitiveSize(),
        };
    }

    void *GetCode() {
        return reinterpret_cast<uint8_t *>(this) + kHeaderOffset +
                GetConstantPrimitiveSize() +
                (kObjectReferenceSize * GetConstantObjectSize());
    }

    mio_buf_t<uint64_t> GetCodeBuf() {
        return {
            .z = static_cast<uint64_t *>(GetCode()),
            .n = GetCodeSize(),
        };
    }

    inline int GetPlacementSize() const {
        return kHeaderOffset + GetConstantPrimitiveSize() +
               GetConstantObjectSize() * kObjectReferenceSize +
               GetCodeSize() * sizeof(uint64_t);
    }

    DECLARE_VM_OBJECT_KIND(NormalFunction)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIONormalFunction)
}; // class NormalFunction

static_assert(sizeof(MIONormalFunction) == sizeof(HeapObject),
              "MIONormalFunction can bigger than HeapObject");


class MIOUpValue final : public HeapObject {
public:
    static const int kFlagsOffset     = kHeapObjectOffset;
    static const int kValueSizeOffset = kFlagsOffset + sizeof(uint32_t);
    static const int kValueOffset     = kValueSizeOffset + sizeof(int);
    static const int kHeaderOffset    = kValueOffset;

    DEFINE_HEAP_OBJ_RW(int, ValueSize)
    DEFINE_HEAP_OBJ_RW(uint32_t, Flags)

    int GetUniqueId() const       { return (GetFlags() >> 1) & 0x7fffffff; }
    bool IsObjectValue() const    { return (GetFlags() & 0x1) != 0; }
    bool IsPrimitiveValue() const { return (GetFlags() & 0x1) == 0; }

    void *GetValue() {
        return reinterpret_cast<uint8_t *>(this) + kValueOffset;
    }

    const void *GetValue() const {
        return reinterpret_cast<const uint8_t *>(this) + kValueOffset;
    }

#define DEFINE_GETTER_SETTER(byte, bit) \
    mio_i##bit##_t GetI##bit() const { \
        DCHECK(IsPrimitiveValue()); \
        return Get<const mio_i##bit##_t>(); \
    } \
    void SetI##bit(mio_i##bit##_t value) { \
        DCHECK(IsPrimitiveValue()); \
        Set<mio_i##bit##_t>(value); \
    }
    MIO_INT_BYTES_TO_BITS(DEFINE_GETTER_SETTER)
#undef  DEFINE_GETTER_SETTER

#define DEFINE_GETTER_SETTER(byte, bit) \
    mio_f##bit##_t GetF##bit() const { \
        DCHECK(IsPrimitiveValue()); \
        return Get<const mio_f##bit##_t>(); \
    } \
    void SetF##bit(mio_f##bit##_t value) { \
        DCHECK(IsPrimitiveValue()); \
        return Set<mio_f##bit##_t>(value); \
    }
    MIO_FLOAT_BYTES_TO_BITS(DEFINE_GETTER_SETTER)
#undef  DEFINE_GETTER_SETTER

    HeapObject *GetObject() const {
        DCHECK(IsObjectValue());
        return Get<HeapObject * const>();
    }

    void SetObject(HeapObject *ob) {
        DCHECK(IsObjectValue());
        Set<HeapObject *>(ob);
    }

    inline int GetPlacementSize() const {
        return kHeaderOffset + GetValueSize();
    }

    DECLARE_VM_OBJECT_KIND(UpValue)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOUpValue)

private:
    template<class T>
    inline T Get() const {
        DCHECK_LE(sizeof(T), GetValueSize());
        return *static_cast<T *>(GetValue());
    }

    template<class T>
    inline void Set(const T &value) {
        DCHECK_LE(sizeof(T), GetValueSize());
        *static_cast<T *>(GetValue()) = value;
    }
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
    static const int kHeaderOffset = kUpValesOffset;

    DEFINE_HEAP_OBJ_RW(uint32_t, Flags)
    DEFINE_HEAP_OBJ_RW(MIOFunction *, Function)
    DEFINE_HEAP_OBJ_RW(int, UpValueSize)

    bool IsOpen() const  { return (GetFlags() & 0x1) == 0; }
    bool IsClose() const { return (GetFlags() & 0x1) != 0; }
    void Close() { SetFlags(GetFlags() | 0x1);     }

    UpValDesc *GetUpValues() {
        return reinterpret_cast<UpValDesc *>(reinterpret_cast<uint8_t *>(this) + kUpValesOffset);
    }

    UpValDesc *GetUpValue(int index) {
        DCHECK_GE(index, 0);
        DCHECK_LT(index, GetUpValueSize());
        return GetUpValues() + index;
    }

    mio_buf_t<UpValDesc> GetUpValuesBuf() {
        return {
            .z = GetUpValues(),
            .n = GetUpValueSize(),
        };
    }

    inline int GetPlacementSize() const {
        return kHeaderOffset + GetUpValueSize() * sizeof(UpValDesc);
    }

    DECLARE_VM_OBJECT_KIND(Closure)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOClosure)
}; // class MIOClosure

static_assert(sizeof(MIOClosure) == sizeof(HeapObject),
              "MIOClosure can bigger than HeapObject");

class MIOUnion final : public HeapObject {
public:
    static const int kTypeInfoOffset = kHeapObjectOffset;
    static const int kDataOffset     = kTypeInfoOffset + kObjectReferenceSize;
    static const int kMIOUnionOffset = kDataOffset + kMaxReferenceValueSize;

    DEFINE_HEAP_OBJ_RW(MIOReflectionType *, TypeInfo)

    void *GetMutableData() {
        return reinterpret_cast<uint8_t *>(this) + kDataOffset;
    }

    const void *GetData() const {
        return reinterpret_cast<const uint8_t *>(this) + kDataOffset;
    }

    HeapObject *GetObject() {
        return *static_cast<HeapObject **>(GetMutableData());
    }

    template<class T>
    inline T GetData() const { return *static_cast<const T *>(GetData()); }

    DECLARE_VM_OBJECT(Union)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOUnion)
}; // class MIOUnion

static_assert(sizeof(MIOUnion) == sizeof(HeapObject),
              "MIOUnion can bigger than HeapObject");

class MIOPair {
public:
    static const int kNextOffset = 0;
    static const int kHeaderOffset = kNextOffset + sizeof(MIOPair *);
    static const int kKeyOffset = kHeaderOffset;
    static const int kValueOffset = kKeyOffset + kMaxReferenceValueSize;
    static const int kMIOPairOffset = kValueOffset + kMaxReferenceValueSize;

    DEFINE_HEAP_OBJ_RW(MIOPair *, Next)

    void *GetKey() { return reinterpret_cast<uint8_t *>(this) + kKeyOffset; }

    void *GetValue() { return reinterpret_cast<uint8_t *>(this) + kValueOffset; }
}; // class MIOMapPair


class MIOHashMap : public HeapObject {
public:
    struct Slot {
        MIOPair *head;
    };

    static const int kDefaultInitialSlots = 4;

    static const int kSeedOffset = kHeapObjectOffset;
    static const int kKeyOffset = kSeedOffset + sizeof(int);
    static const int kValueOffset = kKeyOffset + kObjectReferenceSize;
    static const int kSizeOffset = kValueOffset + kObjectReferenceSize;
    static const int kSlotSizeOffset = kSizeOffset + sizeof(int);
    static const int kSlotsOffset = kSlotSizeOffset + sizeof(int);
    static const int kMIOHashMapOffset = kSlotsOffset + sizeof(Slot *);

    DEFINE_HEAP_OBJ_RW(int, Seed)
    DEFINE_HEAP_OBJ_RW(MIOReflectionType *, Key)
    DEFINE_HEAP_OBJ_RW(MIOReflectionType *, Value)
    DEFINE_HEAP_OBJ_RW(int, Size)
    DEFINE_HEAP_OBJ_RW(int, SlotSize)
    DEFINE_HEAP_OBJ_RW(Slot *, Slots)

    Slot *GetSlot(int index) {
        DCHECK_GE(index, 0);
        DCHECK_LT(index, GetSlotSize());
        return GetSlots() + index;
    }

    DECLARE_VM_OBJECT(HashMap)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOHashMap)
}; // class MIOHashMap

static_assert(sizeof(MIOHashMap) == sizeof(HeapObject),
              "MIOHashMap can bigger than HeapObject");


class MIOError : public HeapObject {
public:
    static const int kLinkedErrorOffset = kHeapObjectOffset;
    static const int kMessageOffset     = kLinkedErrorOffset + kObjectReferenceSize;
    static const int kPositionOffset    = kMessageOffset + sizeof(int);
    static const int kMIOErrorOffset    = kPositionOffset + kObjectReferenceSize;

    DEFINE_HEAP_OBJ_RW(MIOError *, LinkedError)
    DEFINE_HEAP_OBJ_RW(int, Position)
    DEFINE_HEAP_OBJ_RW(MIOString *, Message)

    DECLARE_VM_OBJECT(Error)
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

    bool CanBeKey() const {
        return IsReflectionFloating() || IsReflectionIntegral() ||
               IsReflectionError() || IsReflectionString();
    }

    bool CanNotBeKey() const { return !CanBeKey(); }

    int GetTypePlacementSize() const;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionType)
}; // class MIOReflectionType

class MIOReflectionVoid final : public MIOReflectionType {
public:
    static const int kMIOReflectionVoidOffset = kMIOReflectionTypeOffset;

    DECLARE_VM_OBJECT(ReflectionVoid)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionVoid)
}; // class MIOReflectionVoid

class MIOReflectionIntegral final : public MIOReflectionType {
public:
    static const int kBitWideOffset = kMIOReflectionTypeOffset;
    static const int kMIOReflectionIntegralOffset = kBitWideOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(int, BitWide)

    DECLARE_VM_OBJECT(ReflectionIntegral)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionIntegral)
}; // class MIOReflectionIntegral

class MIOReflectionFloating final : public MIOReflectionType {
public:
    static const int kBitWideOffset = kMIOReflectionTypeOffset;
    static const int kMIOReflectionFloatingOffset = kBitWideOffset + sizeof(int);

    DEFINE_HEAP_OBJ_RW(int, BitWide)

    DECLARE_VM_OBJECT(ReflectionFloating)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionFloating)
}; // class MIOReflectionFloating

class MIOReflectionString final : public MIOReflectionType {
public:
    static const int kMIOReflectionStringOffset = kMIOReflectionTypeOffset;

    DECLARE_VM_OBJECT(ReflectionString)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionString)
}; // class MIOReflectionString

class MIOReflectionError final : public MIOReflectionType {
public:
    static const int kMIOReflectionErrorOffset = kMIOReflectionTypeOffset;

    DECLARE_VM_OBJECT(ReflectionError)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionError)
}; // class MIOReflectionError

class MIOReflectionUnion final : public MIOReflectionType {
public:
    static const int kMIOReflectionUnionOffset = kMIOReflectionTypeOffset;

    DECLARE_VM_OBJECT(ReflectionUnion)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionUnion)
}; // class MIOReflectionUnion

class MIOReflectionMap final : public MIOReflectionType {
public:
    static const int kKeyOffset = kMIOReflectionTypeOffset;
    static const int kValueOffset = kKeyOffset + sizeof(MIOReflectionType *);
    static const int kMIOReflectionMapOffset = kValueOffset + sizeof(MIOReflectionType *);

    DEFINE_HEAP_OBJ_RW(MIOReflectionType *, Key)
    DEFINE_HEAP_OBJ_RW(MIOReflectionType *, Value)

    DECLARE_VM_OBJECT(ReflectionMap)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionMap)
}; // class MIOReflectionMap

class MIOReflectionFunction final : public MIOReflectionType {
public:
    static const int kReturnOffset = kMIOReflectionTypeOffset;
    static const int kNumberOfParametersOffset = kReturnOffset + sizeof(MIOReflectionType *);
    static const int kParamtersOffset = kNumberOfParametersOffset + sizeof(int);
    static const int kHeaderOffset = kParamtersOffset;

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

    inline int GetPlacementSize() const {
        return kHeaderOffset + GetNumberOfParameters() * sizeof(MIOReflectionType *);
    }

    DECLARE_VM_OBJECT_KIND(ReflectionFunction)
    DISALLOW_IMPLICIT_CONSTRUCTORS(MIOReflectionFunction)
}; // class MIOReflectionFunction

////////////////////////////////////////////////////////////////////////////////
/// Toolkit:
////////////////////////////////////////////////////////////////////////////////
struct MIOStringDataHash {
    std::size_t operator()(const char *data) const {
        std::size_t h = 1315423911;
        while (*data) {
            h ^= ((h << 5) + *(data++) + (h >> 2));
        }
        return h;
    }
};

struct MIOStringDataEqualTo {
    bool operator() (const char *val1, const char *val2) const {
        return val1 == val2 ? true : strcmp(val1, val2) == 0;
    }
};

////////////////////////////////////////////////////////////////////////////////
/// Inline Functions:
////////////////////////////////////////////////////////////////////////////////

inline void HORemove(HeapObject *ob) {
    DCHECK_NOTNULL(ob->GetNext())->SetPrev(ob->GetPrev());
    DCHECK_NOTNULL(ob->GetPrev())->SetNext(ob->GetNext());
    ob->SetNext(nullptr); // for debugging
    ob->SetPrev(nullptr); // for debugging
}

inline void HOInsertHead(HeapObject *h, HeapObject *x) {
    x->SetNext(h->GetNext());
    x->GetNext()->SetPrev(x);
    x->SetPrev(h);
    h->SetNext(x);
}

inline void HOInsertTail(HeapObject *h, HeapObject *x) {
    x->SetPrev(h->GetPrev());
    x->GetPrev()->SetNext(x);
    x->SetNext(h);
    h->SetPrev(x);
}

inline void HOLink(HeapObject *h, HeapObject *n) {
    h->GetPrev()->SetNext(n->GetNext());
    n->GetNext()->SetPrev(h->GetPrev());
    h->SetPrev(n->GetPrev());
    h->GetPrev()->SetNext(h);
}

inline bool HOIsEmpty(HeapObject *h) {
    return h->GetNext() == h;
}

inline bool HOIsNotEmpty(HeapObject *h) {
    return !HOIsEmpty(h);
}

} // namespace mio

#endif // MIO_VM_OBJECTS_H_
