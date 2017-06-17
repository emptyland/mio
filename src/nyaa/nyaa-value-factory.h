#ifndef MIO_NYAA_VALUE_FACTORY_H_
#define MIO_NYAA_VALUE_FACTORY_H_

#include "nyaa-instructions.h"

namespace mio {

class NValueFactory {
public:
    NValueFactory(Zone *zone)
        : zone_(DCHECK_NOTNULL(zone)) {}

    NAdd *CreateAdd(NType type, NValue *lhs, NValue *rhs) {
        return new (zone_) NAdd(type, lhs, rhs);
    }
private:
    Zone *zone_;
};

} // namespace mio

#endif // MIO_NYAA_VALUE_FACTORY_H_
