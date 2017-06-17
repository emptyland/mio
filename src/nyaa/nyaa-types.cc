#include "nyaa-types.h"

namespace mio {

const char * const kNTypeShortNames[] = {
#define DEFINE_ELEMENT(name, short_name) #short_name,
    NYAA_TYPES(DEFINE_ELEMENT)
#undef DEFINE_ELEMENT
};

} // namespace mio
