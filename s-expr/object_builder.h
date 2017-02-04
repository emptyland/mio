#ifndef MIO_S_EXPR_OBJECT_BUILDER_H_
#define MIO_S_EXPR_OBJECT_BUILDER_H_

#include "object.h"
#include "base.h"

namespace mio {

class ObjectBuilder {
public:
    ObjectBuilder();
    virtual ~ObjectBuilder();

    virtual Number *NewNumber(NativeNumber value) = 0;
    virtual Pair *NewPair(Object *car, Pair *cdr) = 0;
    virtual Id *NewId(const char *z, int16_t len, int16_t tag) = 0;
    virtual Boolean *NewBoolean(NativeBoolean value) = 0;

    virtual void DeleteObject(const Object *obj) = 0;

    inline Id *NewId(const char *z, int16_t len) {
        return NewId(z, len, 0);
    }

    inline Boolean *NewTrue()  { return NewBoolean(kNativeTrue); }
    inline Boolean *NewFalse() { return NewBoolean(kNativeFalse); }
    inline Boolean *NewUndef() { return NewBoolean(kNativeUndef); }

    DISALLOW_IMPLICT_CONSTRUCTORS(ObjectBuilder)

protected:
    template<class T>
    inline void SetObjectProperty(Object *obj, int offset, T value) {
        *obj->raw_field_address<T>(offset) = value;
    }

    inline void *GetObjectPropertyPointer(Object *obj, int offset) {
        return obj->raw_field_pointer(offset);
    }
};

ObjectBuilder *NewMallocObjectBuilder(bool ownership);

} // namespace mio

#endif // MIO_S_EXPR_OBJECT_BUILDER_H_
