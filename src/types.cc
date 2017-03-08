#include "types.h"

namespace mio {

namespace {

class TypeDigest {
public:
    static const int64_t kInitialValue = 1315423911;

    TypeDigest() = default;

    int64_t value() const { return value_; }

    void Step(int64_t atomic) {
        value_ ^= ((value_ << 5) + atomic + (value_ >> 2));
    }

private:
    int64_t value_ = kInitialValue;
};

}

TypeFactory::TypeFactory(Zone *zone)
    : zone_(DCHECK_NOTNULL(zone))
    , void_type_(new (zone) Void(TOKEN_VOID))
    , unknown_type_(new (zone) Unknown(-1))
    , string_type_(new (zone) String(TOKEN_STRING)) {
    integral_types_[0] = new (zone_) Integral(1,  TOKEN_BOOL);
    integral_types_[1] = new (zone_) Integral(8,  TOKEN_I8);
    integral_types_[2] = new (zone_) Integral(16, TOKEN_I16);
    integral_types_[3] = new (zone_) Integral(32, TOKEN_I32);
    integral_types_[4] = new (zone_) Integral(64, TOKEN_I64);
}

FunctionPrototype *
TypeFactory::GetFunctionPrototype(ZoneVector<Paramter *> *paramters,
                                  Type *return_type) {
    auto prototype = new (zone_) FunctionPrototype(paramters, return_type, 0);
    prototype->UpdateId();
    return prototype;
}

void FunctionPrototype::UpdateId() {
    TypeDigest digest;

    digest.Step(TOKEN_FUNCTION);
    for (int i = 0; i < paramters_->size(); ++i) {
        digest.Step(paramters_->At(i)->param_type()->id());
    }
    digest.Step(return_type()->id() << 4);
}

} // namespace mio
