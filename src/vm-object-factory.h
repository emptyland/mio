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
class MIOReflectionMap;
class MIOReflectionFunction;

class MIOHashMapSurface;
template<class K, class V> class MIOHashMapStub;

class ObjectFactory {
public:
    ObjectFactory() = default;
    virtual  ~ObjectFactory() = default;

    inline Handle<MIOString> GetOrNewString(const char *z, int n);

    inline Handle<MIOString> GetOrNewString(const char *z) {
        return GetOrNewString(z, static_cast<int>(strlen(z)));
    }

    virtual Handle<MIOString> GetOrNewString(const mio_strbuf_t *buf, int n) = 0;

    virtual Handle<MIONativeFunction>
    CreateNativeFunction(const char *signature, MIOFunctionPrototype pointer) = 0;

    virtual
    Handle<MIONormalFunction>
    CreateNormalFunction(const std::vector<Handle<HeapObject>> &constant_objects,
                         const void *constant_primitive,
                         int constant_primitive_size,
                         const void *code,
                         int code_size) = 0;

    virtual Handle<MIOClosure> CreateClosure(Handle<MIOFunction> function,
                                             int up_values_size) = 0;

    virtual Handle<MIOHashMap> CreateHashMap(int seed, int initial_slots,
                                             Handle<MIOReflectionType> key,
                                             Handle<MIOReflectionType> value) = 0;

    virtual Handle<MIOError> CreateError(const char *message, int position,
                                        Handle<MIOError> linked) = 0;

    virtual Handle<MIOUnion> CreateUnion(const void *data, int size,
                                         Handle<MIOReflectionType> type_info) = 0;

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

    virtual Handle<MIOReflectionString> CreateReflectionString(int64_t tid) = 0;

    virtual Handle<MIOReflectionError> CreateReflectionError(int64_t tid) = 0;

    virtual Handle<MIOReflectionUnion> CreateReflectionUnion(int64_t tid) = 0;

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

} // namespace mio

#endif // MIO_VM_OBJECT_FACTORY_H_
