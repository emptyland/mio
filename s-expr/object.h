#ifndef MIO_S_EXPR_OBJECT_H_
#define MIO_S_EXPR_OBJECT_H_

#include <stdint.h>
#include <stdint.h>
#include <string>

namespace mio {

// Expression: (op arg1 arg2 ...)
// Lambda:     (lambda (args) (expr1) (expr2) ...)
// Let:        (let a (expr))
// Def:        (def name (args) (expr1) (expr2) ...)
//
// (def foo (a b c)
//     (let r1 a b)
//     (let r2 r1 c)
//     (+ r1 r2))
//

#define DECL_VAL_KIND(_) \
    _(ID,     Id)        \
    _(NUMBER, Number)    \
    _(PAIR,   Pair)

#define DEF_VAL_KIND(name, clazz) \
    VALUE_##name,
enum ValueKind {
    DECL_VAL_KIND(DEF_VAL_KIND)
};
#undef DEF_VAL_KIND


#define DEFINE_OBJ_PROPERTY(name, type) \
    inline type name() { return *raw_field_address<type>(k##name##Offset); }

static const int kPointerSize = sizeof(void*);

typedef float NativeNumber;

class Object {
public:
    static const int kObjectOffset = 0;
    static const int kkindOffset = kObjectOffset;
    static const int kSize = kkindOffset + sizeof(int);

    DEFINE_OBJ_PROPERTY(kind, int)
protected:
    template<class T>
    inline T *raw_field_address(int offset) {
        return static_cast<T *>(raw_field_pointer(offset));
    }

    void *raw_field_pointer(int offset) {
       return static_cast<uint8_t*>(static_cast<void *>(this)) + offset;
    }
};

class Pair : public Object {
public:
    static const int kPairOffset = Object::kSize;
    static const int kcarOffset  = kPairOffset;
    static const int kcdrOffset  = kcarOffset + kPointerSize;
    static const int kSize       = kcdrOffset + kPointerSize;

    DEFINE_OBJ_PROPERTY(car, Object*)
    DEFINE_OBJ_PROPERTY(cdr, Object*)
};

class Id : public Object {
public:
    static const int kIdOffset   = Object::kSize;
    static const int klenOffset  = kIdOffset;
    static const int ktagOffset  = klenOffset + sizeof(int16_t);
    static const int ktxtOffset  = ktagOffset + sizeof(int16_t);
    static const int kHeaderSize = ktxtOffset;

    DEFINE_OBJ_PROPERTY(len, int16_t)
    DEFINE_OBJ_PROPERTY(tag, int16_t)
    const char *c_str() { return raw_field_address<const char>(ktxtOffset); }
    int size() { return kHeaderSize + len(); }
};

class Number : public Object {
public:
    static const int kNumberOffset = Object::kSize;
    static const int knativeOffset = kNumberOffset;
    static const int kSize         = knativeOffset + sizeof(NativeNumber);

    NativeNumber native();
};

} // namespace mio

#endif // MIO_S_EXPR_OBJECT_H_
