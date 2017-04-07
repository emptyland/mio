#ifndef MIO_VM_OBJECT_FACTORY_H_
#define MIO_VM_OBJECT_FACTORY_H_

#include "vm.h"
#include "handles.h"
#include "base.h"
#include <vector>

namespace mio {

class MIOString;
class MIONativeFunction;
class MIONormalFunction;
class MIOHashMap;
class MIOError;
class MIOReflectionType;
class MIOReflectionVoid;
class MIOReflectionIntegral;
class MIOReflectionFloating;
class MIOReflectionString;
class MIOReflectionError;
class MIOReflectionUnion;
class MIOReflectionMap;
class MIOReflectionFunction;

class ObjectFactory {
public:
    ObjectFactory() = default;
    virtual  ~ObjectFactory() = default;

    inline Handle<MIOString> CreateString(const char *z, int n);

    virtual Handle<MIOString> GetOrNewString(const char *z, int n, int **offset) = 0;

    virtual Handle<MIOString> CreateString(const mio_strbuf_t *buf, int n) = 0;

    virtual Handle<MIONativeFunction>
    CreateNativeFunction(const char *signature, MIOFunctionPrototype pointer) = 0;

    virtual Handle<MIONormalFunction> CreateNormalFunction(const void *code, int size) = 0;

    virtual Handle<MIOHashMap> CreateHashMap(int seed, uint32_t flags) = 0;

    virtual Handle<MIOError> CreateError(const char *message, int position,
                                        Handle<MIOError> linked) = 0;

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

inline Handle<MIOString> ObjectFactory::CreateString(const char *z, int n) {
    mio_strbuf_t buf = { .z = z, .n = n };
    return CreateString(&buf, 1);
}

} // namespace mio

#endif // MIO_VM_OBJECT_FACTORY_H_
