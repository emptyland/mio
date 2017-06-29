#ifndef MIO_VM_OBJECT_FACTORY_H_
#define MIO_VM_OBJECT_FACTORY_H_

#include "vm.h"
#include "vm-objects.h"
#include "handles.h"
#include "base.h"
#include <vector>

namespace mio {

class HeapObject;
class MIOString;
class MIOClosure;
class MIOHashMap;
class MIOError;
class MIOUnion;
class MIOExternal;
class MIOUpValue;
class MIOFunction;
class MIONativeFunction;
class MIONormalFunction;

class MIOReflectionType;
class MIOReflectionVoid;
class MIOReflectionIntegral;
class MIOReflectionFloating;
class MIOReflectionString;
class MIOReflectionError;
class MIOReflectionUnion;
class MIOReflectionExternal;
class MIOReflectionArray;
class MIOReflectionSlice;
class MIOReflectionMap;
class MIOReflectionFunction;


class ObjectFactory {
public:
    ObjectFactory() = default;
    virtual  ~ObjectFactory() = default;

    inline Handle<MIOString> GetOrNewString(const char *z, int n);

    inline Handle<MIOString> GetOrNewString(const char *z) {
        return GetOrNewString(z, static_cast<int>(strlen(z)));
    }

    template<class T>
    inline Handle<MIOExternal> NewExternalTemplate(T *value);

    virtual ManagedAllocator *allocator() = 0;

    virtual Handle<MIOString> GetOrNewString(const mio_strbuf_t *buf, int n) = 0;

    virtual Handle<MIONativeFunction>
    CreateNativeFunction(const char *signature, MIOFunctionPrototype pointer) = 0;

    virtual
    Handle<MIONormalFunction>
    CreateNormalFunction(const std::vector<Handle<HeapObject>> &constant_objects,
                         const void *constant_primitive,
                         int constant_primitive_size,
                         const void *code,
                         int code_size,
                         int id) = 0;

    virtual Handle<MIOClosure> CreateClosure(Handle<MIOFunction> function,
                                             int up_values_size) = 0;

    virtual Handle<MIOVector> CreateVector(int initial_size,
                                           Handle<MIOReflectionType> element) = 0;

    virtual Handle<MIOSlice> CreateSlice(int begin, int size,
                                         Handle<HeapObject> core) = 0;

    virtual Handle<MIOHashMap> CreateHashMap(int seed, int initial_slots,
                                             Handle<MIOReflectionType> key,
                                             Handle<MIOReflectionType> value) = 0;

    virtual Handle<MIOError> CreateError(Handle<MIOString> msg,
                                         Handle<MIOString> file_name,
                                         int position,
                                         Handle<MIOError> linked) = 0;

    inline Handle<MIOError> CreateError(const char *msg, const char *file_name,
                                        int position, Handle<MIOError> linked);

    virtual Handle<MIOUnion> CreateUnion(const void *data, int size,
                                         Handle<MIOReflectionType> type_info) = 0;

    virtual Handle<MIOExternal> CreateExternal(intptr_t type_code, void *value) = 0;

    virtual Handle<MIOUpValue> GetOrNewUpValue(const void *data, int size,
                                               int unique_id, bool is_primitive) = 0;

    //
    // Reflection Type Objects:
    //
    virtual Handle<MIOReflectionVoid> CreateReflectionVoid(int64_t tid) = 0;

    virtual
    Handle<MIOReflectionIntegral>
    CreateReflectionIntegral(int64_t tid, int bitwide) = 0;

    virtual
    Handle<MIOReflectionFloating>
    CreateReflectionFloating(int64_t tid, int bitwide) = 0;

    virtual Handle<MIOReflectionRef> CreateReflectionRef(int64_t tid) = 0;

    virtual Handle<MIOReflectionString> CreateReflectionString(int64_t tid) = 0;

    virtual Handle<MIOReflectionError> CreateReflectionError(int64_t tid) = 0;

    virtual Handle<MIOReflectionUnion> CreateReflectionUnion(int64_t tid) = 0;

    virtual Handle<MIOReflectionExternal> CreateReflectionExternal(int64_t tid) = 0;

    virtual
    Handle<MIOReflectionArray>
    CreateReflectionArray(int64_t tid, Handle<MIOReflectionType> element) = 0;

    virtual
    Handle<MIOReflectionSlice>
    CreateReflectionSlice(int64_t tid, Handle<MIOReflectionType> element) = 0;

    virtual
    Handle<MIOReflectionMap>
    CreateReflectionMap(int64_t tid, Handle<MIOReflectionType> key,
                        Handle<MIOReflectionType> value) = 0;

    virtual
    Handle<MIOReflectionFunction>
    CreateReflectionFunction(int64_t tid, Handle<MIOReflectionType> return_type,
                             int number_of_parameters,
                             const std::vector<Handle<MIOReflectionType>> &parameters) = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(ObjectFactory)
}; // class ObjectFactory

inline Handle<MIOString> ObjectFactory::GetOrNewString(const char *z, int n) {
    mio_strbuf_t buf = { .z = z, .n = n };
    return GetOrNewString(&buf, 1);
}

inline Handle<MIOError> ObjectFactory::CreateError(const char *msg,
                                                   const char *file_name,
                                                   int position,
                                                   Handle<MIOError> linked) {
    auto s = GetOrNewString(msg);
    auto f = file_name ? GetOrNewString(file_name) : GetOrNewString("");
    return CreateError(s, f, position, linked);
}

template<class T>
inline Handle<MIOExternal> ObjectFactory::NewExternalTemplate(T *value) {
    ExternalGenerator<T> generator;
    return CreateExternal(generator.type_code(), value);
}

} // namespace mio

#endif // MIO_VM_OBJECT_FACTORY_H_
