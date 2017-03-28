#ifndef MIO_VM_OBJECT_FACTORY_H_
#define MIO_VM_OBJECT_FACTORY_H_

#include "vm.h"
#include "handles.h"
#include "base.h"

namespace mio {

class MIOString;
class MIONativeFunction;
class MIONormalFunction;
class MIOHashMap;

class ObjectFactory {
public:
    ObjectFactory() = default;
    virtual  ~ObjectFactory() = default;

    virtual Local<MIOString> CreateString(const char *z, int n) = 0;

    virtual Local<MIOString> GetOrNewString(const char *z, int n, int **offset) = 0;

    virtual Local<MIONativeFunction>
    CreateNativeFunction(const char *signature, MIOFunctionPrototype pointer) = 0;

    virtual Local<MIONormalFunction> CreateNormalFunction(int address) = 0;

    virtual Local<MIOHashMap> CreateHashMap(int seed, uint32_t flags) = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(ObjectFactory)
};

} // namespace mio

#endif // MIO_VM_OBJECT_FACTORY_H_
