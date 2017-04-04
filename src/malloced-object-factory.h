#ifndef MIO_MALLOCED_OBJECT_FACTORY_H_
#define MIO_MALLOCED_OBJECT_FACTORY_H_

#include "vm-object-factory.h"
#include <vector>

namespace mio {

class HeapObject;

class MallocedObjectFactory : public ObjectFactory {
public:
    MallocedObjectFactory();
    virtual ~MallocedObjectFactory() override;

    virtual Local<MIOString> CreateString(const char *z, int n) override;

    virtual Local<MIOString>
    GetOrNewString(const char *z, int n, int **offset) override;

    virtual Local<MIONativeFunction>
    CreateNativeFunction(const char *signature,
                         MIOFunctionPrototype pointer) override;

    virtual Local<MIONormalFunction>
    CreateNormalFunction(const void *code, int size) override;

    virtual Local<MIOHashMap>
    CreateHashMap(int seed, uint32_t flags) override;

    virtual
    Local<MIOError> CreateError(const char *message, int position,
                                Local<MIOError> linked) override;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MallocedObjectFactory)
private:
    std::vector<HeapObject *> objects_;
    int  offset_stub_;
    int *offset_pointer_;
};

} // namespace mio

#endif // MIO_MALLOCED_OBJECT_FACTORY_H_
