#ifndef MIO_S_EXPR_EVALUATE_H_
#define MIO_S_EXPR_EVALUATE_H_

#include "object.h"
#include "base.h"

namespace mio {

class Env;
class Object;
class Pair;

class Evaluate {
public:
    Object *Apply(Pair *expr, Env *env, bool *ok);
    Object *Eval(Object *obj, Env *env, bool *ok);

    NativeNumber EnsureNativeNumber(Object *obj, Env *env, bool *ok);

    DISALLOW_IMPLICT_CONSTRUCTORS(Evaluate)

private:
    inline Object *Car(Object *obj, Env *env) {
        if (!obj->is_pair()) {
            return env->Undef();
        }
        return static_cast<Pair *>(obj)->car();
    }

    inline Object *Cdr(Object *obj, Env *env) {
        if (!obj->is_pair()) {
            return env->Undef();
        }
        return static_cast<Pair *>(obj)->cdr();
    }

    inline Object *Cdar(Object *obj, Env *env) {
        return Car(Cdr(obj, env), env);
    }

    inline Object *Cddr(Object *obj, Env *env) {
        return Cdr(Cdr(obj, env), env);
    }

#define DEF_ASSERT_METHOD(name, clazz) \
    inline clazz *Assert##clazz(Object *obj, bool *ok) { \
        if (obj->kind() != VALUE_##name) {               \
            *ok = false;                                 \
            return nullptr;                              \
        }                                                \
        return static_cast<clazz *>(obj);                \
    }

    DECL_VAL_KIND(DEF_ASSERT_METHOD)

#undef DEF_ASSERT_METHOD
};

} // namspace mio

#endif // MIO_S_EXPR_EVALUATE_H_
