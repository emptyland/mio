#include "types.h"

namespace mio {

TypeFactory::TypeFactory(Zone *zone)
    : zone_(DCHECK_NOTNULL(zone))
    , void_type_(new (zone) Void(0))
    , unknown_type_(new (zone) Type(-1)) {
    integral_types_[0] = new (zone_) Integral(1,  1);
    integral_types_[1] = new (zone_) Integral(8,  2);
    integral_types_[2] = new (zone_) Integral(16, 3);
    integral_types_[3] = new (zone_) Integral(32, 4);
    integral_types_[4] = new (zone_) Integral(64, 5);
}

FunctionPrototype *
TypeFactory::GetFunctionPrototype(ZoneVector<Paramter *> *paramters,
                                  Type *return_type) {

    return new (zone_) FunctionPrototype(paramters, return_type, 0);
}

} // namespace mio
