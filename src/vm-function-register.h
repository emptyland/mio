#ifndef VM_FUNCTION_REGISTER_H_
#define VM_FUNCTION_REGISTER_H_

#include "vm-objects.h"
#include "base.h"
#include <vector>

namespace mio {

class FunctionEntry;
class CodeCache;

class FunctionRegister {
public:
    FunctionRegister(CodeCache *code_cache)
        : code_cache_(DCHECK_NOTNULL(code_cache)) {}

    virtual ~FunctionRegister() = default;

    inline bool RegisterNativeFunction(const char *name,
                                       MIOFunctionPrototype pointer);

    template<class F>
    inline bool RegisterFunctionTemplate(const char *name, F *pointer);

    virtual FunctionEntry *FindOrInsert(const char *name) = 0;

    virtual FunctionEntry *FindOrNull(const char *name) const = 0;

    virtual Handle<MIONativeFunction> FindNativeFunction(const char *name) = 0;

    virtual int GetAllFunctions(std::vector<Handle<MIONormalFunction>> *all_functions) = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionRegister)

private:
    void **BuildWarper(MIOString *signature);

    CodeCache *code_cache_;
}; // class FunctionRegister

inline bool FunctionRegister::RegisterNativeFunction(const char *name,
                                                     MIOFunctionPrototype pointer) {
    auto fn = FindNativeFunction(name);
    if (!fn.empty()) {
        fn->SetNativePointer(DCHECK_NOTNULL(pointer));
    }
    return !fn.empty();
}

////////////////////////////////////////////////////////////////////////////////
/// NativeValue analyer
////////////////////////////////////////////////////////////////////////////////

/*
#define DEFINE_TYPE_NODES(M) \
    M(Slice,    's') \
    M(Array,    'a') \
    M(Map,      'm') \
    M(FunctionPrototype, 'r') \
    M(Union,    'u') \
    M(Integral, 'I') \
    M(Floating, 'F') \
    M(String,   'z') \
    M(Void,     '!') \
    M(Error,    'e') \
    M(Unknown,  '\0')

 * Integral Signature
 * i1  - 1
 * i8  - 8
 * i16 - 7
 * i32 - 5
 * i64 - 9
 *
 * Floating Signature
 * f32 - 3
 * f64 - 6
 */

template<class T>
struct NativeValue {};

template<>
struct NativeValue<mio_i8_t> {
    inline bool Check(char s) { return s == '8'; }
};

template<>
struct NativeValue<mio_i16_t> {
    inline bool Check(char s) { return s == '7'; }
};

template<>
struct NativeValue<mio_i32_t> {
    inline bool Check(char s) { return s == '5'; }
};

template<>
struct NativeValue<mio_i64_t> {
    inline bool Check(char s) { return s == '9'; }
};

template<>
struct NativeValue<mio_f32_t> {
    inline bool Check(char s) { return s == '3'; }
};

template<>
struct NativeValue<mio_f64_t> {
    inline bool Check(char s) { return s == '6'; }
};

template<>
struct NativeValue<void> {
    inline bool Check(char s) { return s == '!'; }
};

template<>
struct NativeValue<HeapObject*> {
    inline bool Check(char s) {
        return s == 's' || s == 'a' || s == 'm' || s == 'r' || s == 'u' ||
               s == 'z' || s == 'e';
    }
};

template<>
struct NativeValue<MIOString *> {
    inline bool Check(char s) { return s == 'z'; }
};

////////////////////////////////////////////////////////////////////////////////
/// FunctionTemplate
////////////////////////////////////////////////////////////////////////////////

struct FunctionTemplateBase {
    FunctionTemplateBase(Handle<MIONativeFunction> fn)
        : fn_(fn) {}

    Handle<MIONativeFunction> fn_;
};

template<class F>
struct FunctionTemplate : public FunctionTemplateBase {

};

template<class R>
struct FunctionTemplate<R(Thread *)> : public FunctionTemplateBase {

    FunctionTemplate(Handle<MIONativeFunction> fn)
        : FunctionTemplateBase(fn) {}

    inline bool Check() const {
        auto sign = fn_->GetSignature()->Get();
        return NativeValue<R>().Check(sign.z[0]);
    }
};

template<class R, class A1>
struct FunctionTemplate<R(Thread *, A1)> : public FunctionTemplateBase {

    FunctionTemplate(Handle<MIONativeFunction> fn)
        : FunctionTemplateBase(fn) {}

    inline bool Check() const {
        auto sign = fn_->GetSignature()->Get();
        if (!NativeValue<R>().Check(sign.z[0])) {
            return false;
        }
        if (sign.n != 3) {
            return false;
        }
        return NativeValue<A1>().Check(sign.z[2]);
    }
};

template<class R, class A1, class A2>
struct FunctionTemplate<R(Thread *, A1, A2)> : public FunctionTemplateBase {

    FunctionTemplate(Handle<MIONativeFunction> fn)
    : FunctionTemplateBase(fn) {}

    inline bool Check() const {
        auto sign = fn_->GetSignature()->Get();
        if (!NativeValue<R>().Check(sign.z[0])) {
            return false;
        }
        if (sign.n != 4) {
            return false;
        }
        if (!NativeValue<A1>().Check(sign.z[2])) {
            return false;
        }
        return NativeValue<A2>().Check(sign.z[3]);
    }
};

template<class F>
inline
bool FunctionRegister::RegisterFunctionTemplate(const char *name, F *pointer) {
    auto fn = FindNativeFunction(name);
    if (fn.empty()) {
        DLOG(ERROR) << "function: " << name << " not found!";
        return false;
    }
    FunctionTemplate<F> tmpl(fn);
    if (!tmpl.Check()) {
        return false;
    }

    auto warper = BuildWarper(fn->GetSignature());
    if (!warper) {
        return false;
    }
    fn->SetTemplate(reinterpret_cast<void *>(pointer));
    fn->SetNativeWarperIndex(warper);
    return true;
}

} // namespace mio

#endif // VM_FUNCTION_REGISTER_H_
