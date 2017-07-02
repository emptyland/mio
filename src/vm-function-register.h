#ifndef VM_FUNCTION_REGISTER_H_
#define VM_FUNCTION_REGISTER_H_

#include "object-traits.h"
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

    virtual int GetAllFunctions(std::vector<Handle<MIOGeneratedFunction>> *all_functions) = 0;

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
/// FunctionTemplate
////////////////////////////////////////////////////////////////////////////////

struct FunctionTemplateBase {
    FunctionTemplateBase(Handle<MIONativeFunction> fn)
        : fn_(fn) {}

    Handle<MIONativeFunction> fn_;
};

template<class F>
struct FunctionTemplate : public FunctionTemplateBase {};

template<class R>
struct FunctionTemplate<R(Thread *)> : public FunctionTemplateBase {

    FunctionTemplate(Handle<MIONativeFunction> fn)
        : FunctionTemplateBase(fn) {}

    inline bool Check() const {
        auto sign = fn_->GetSignature()->Get();
        return NativeValue<R>().Check(sign.z[0]);
    }
};

#define TEMPLATE_CHECK_ARG(index) \
    if (!NativeValue<A##index>().Check(sign.z[(index + 1)])) { \
        DLOG(ERROR) << "argument[" << (index) << "] type error, unexpected: " \
                    << NativeValue<A##index>().type(); \
        return false; \
    } (void)0

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
        TEMPLATE_CHECK_ARG(1);
        return true;
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
        TEMPLATE_CHECK_ARG(1);
        TEMPLATE_CHECK_ARG(2);
        return true;
    }
};

template<class R, class A1, class A2, class A3>
struct FunctionTemplate<R(Thread *, A1, A2, A3)> : public FunctionTemplateBase {

    FunctionTemplate(Handle<MIONativeFunction> fn)
    : FunctionTemplateBase(fn) {}

    inline bool Check() const {
        auto sign = fn_->GetSignature()->Get();
        if (!NativeValue<R>().Check(sign.z[0])) {
            return false;
        }
        if (sign.n != 5) {
            return false;
        }
        TEMPLATE_CHECK_ARG(1);
        TEMPLATE_CHECK_ARG(2);
        TEMPLATE_CHECK_ARG(3);
        return true;
    }
};

template<class R, class A1, class A2, class A3, class A4>
struct FunctionTemplate<R(Thread *, A1, A2, A3, A4)> : public FunctionTemplateBase {

    FunctionTemplate(Handle<MIONativeFunction> fn)
    : FunctionTemplateBase(fn) {}

    inline bool Check() const {
        auto sign = fn_->GetSignature()->Get();
        if (!NativeValue<R>().Check(sign.z[0])) {
            return false;
        }
        if (sign.n != 6) {
            return false;
        }
        TEMPLATE_CHECK_ARG(1);
        TEMPLATE_CHECK_ARG(2);
        TEMPLATE_CHECK_ARG(3);
        TEMPLATE_CHECK_ARG(4);
        return true;
    }
};

template<class R, class A1, class A2, class A3, class A4, class A5>
struct FunctionTemplate<R(Thread *, A1, A2, A3, A4, A5)> : public FunctionTemplateBase {

    FunctionTemplate(Handle<MIONativeFunction> fn)
    : FunctionTemplateBase(fn) {}

    inline bool Check() const {
        auto sign = fn_->GetSignature()->Get();
        if (!NativeValue<R>().Check(sign.z[0])) {
            return false;
        }
        if (sign.n != 7) {
            return false;
        }
        TEMPLATE_CHECK_ARG(1);
        TEMPLATE_CHECK_ARG(2);
        TEMPLATE_CHECK_ARG(3);
        TEMPLATE_CHECK_ARG(4);
        TEMPLATE_CHECK_ARG(5);
        return true;
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
