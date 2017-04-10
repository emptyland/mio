#include "vm-objects.h"

namespace mio {

int MIOReflectionType::GetPlacementSize() const {
    switch (GetKind()) {
        case HeapObject::kReflectionIntegral:
            return (AsReflectionIntegral()->GetBitWide() + 7) / 8;

        case HeapObject::kReflectionFloating:
            return (AsReflectionFloating()->GetBitWide() + 7) / 8;

        case HeapObject::kReflectionMap:
        case HeapObject::kReflectionError:
        case HeapObject::kReflectionUnion:
        case HeapObject::kReflectionString:
        case HeapObject::kReflectionFunction:
            return kObjectReferenceSize;

        case HeapObject::kReflectionVoid:
            DLOG(FATAL) << "has no size for void reflection type.";
            return 0;

        default:
            DLOG(FATAL) << "noreached! this kind not be supported. " << GetKind();
            return 0;
    }
}

} // namespace mio
