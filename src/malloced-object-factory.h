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

    virtual Handle<MIOString> CreateString(const mio_strbuf_t *bufs, int n) override;

    virtual Handle<MIOString>
    GetOrNewString(const char *z, int n, int **offset) override;

    virtual Handle<MIOClosure>
    CreateClosure(Handle<MIOFunction> function, int up_values_size) override;

    virtual Handle<MIONativeFunction>
    CreateNativeFunction(const char *signature,
                         MIOFunctionPrototype pointer) override;

    virtual Handle<MIONormalFunction>
    CreateNormalFunction(const std::vector<Handle<HeapObject>> &constant_objects,
                         const void *constant_primitive,
                         int constant_primitive_size,
                         const void *code,
                         int code_size) override;

    virtual Handle<MIOHashMap>
    CreateHashMap(int seed, uint32_t flags) override;

    virtual
    Handle<MIOError> CreateError(const char *message, int position,
                                Handle<MIOError> linked) override;

    virtual
    Handle<MIOUnion> CreateUnion(const void *data, int size,
                                 Handle<MIOReflectionType> type_info) override;

    virtual
    Handle<MIOReflectionVoid> CreateReflectionVoid(int64_t tid) override;

    virtual
    Handle<MIOReflectionIntegral>
    CreateReflectionIntegral(int64_t tid, int bitwide) override;

    virtual
    Handle<MIOReflectionFloating>
    CreateReflectionFloating(int64_t tid, int bitwide) override;

    virtual Handle<MIOReflectionString>
    CreateReflectionString(int64_t tid) override;

    virtual Handle<MIOReflectionError>
    CreateReflectionError(int64_t tid) override;

    virtual Handle<MIOReflectionUnion>
    CreateReflectionUnion(int64_t tid) override;

    virtual
    Handle<MIOReflectionMap>
    CreateReflectionMap(int64_t tid,
                        Handle<MIOReflectionType> key,
                        Handle<MIOReflectionType> value) override;

    virtual
    Handle<MIOReflectionFunction>
    CreateReflectionFunction(int64_t tid, Handle<MIOReflectionType> return_type, 
                             int number_of_parameters,
                             const std::vector<Handle<MIOReflectionType>> &parameters) override;

    DISALLOW_IMPLICIT_CONSTRUCTORS(MallocedObjectFactory)
private:
    std::vector<HeapObject *> objects_;
    int  offset_stub_;
    int *offset_pointer_;
};

} // namespace mio

#endif // MIO_MALLOCED_OBJECT_FACTORY_H_
