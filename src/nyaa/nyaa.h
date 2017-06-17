#ifndef MIO_NYAA_H_
#define MIO_NYAA_H_

#include "zone-vector.h"
#include "raw-string.h"
#include "zone.h"

namespace mio {

class NPhi;
class NInstruction;

class NBasicBlock : public ManagedObject {
public:
    DEF_GETTER(int, id)
    DEF_PROP_RW(RawStringRef, name)
    DEF_ZONE_VECTOR_PROP_RWA(NPhi *, phi)
    DEF_ZONE_VECTOR_PROP_RWA(NBasicBlock *, prev_block)
    DEF_ZONE_VECTOR_PROP_RWA(NBasicBlock *, next_block)

private:
    int                       id_;
    RawStringRef              name_ = RawString::kEmpty;
    ZoneVector<NPhi *>        phis_;
    NInstruction             *first_;
    NInstruction             *last_;
    ZoneVector<NBasicBlock *> prev_blocks_;
    ZoneVector<NBasicBlock *> next_blocks_;
}; // class NBasicBlock

} // namespace mio

#endif // MIO_NYAA_H_
