#ifndef MIO_VM_OBJECT_EXTRA_FACTORY_H_
#define MIO_VM_OBJECT_EXTRA_FACTORY_H_

#include "vm-objects.h"
#include "managed-allocator.h"
#include "raw-string.h"

namespace mio {

class ObjectExtraFactory {
public:
    ObjectExtraFactory(ManagedAllocator *allocator)
        : allocator_(DCHECK_NOTNULL(allocator)) {}

    FunctionDebugInfo *CreateFunctionDebugInfo(RawStringRef unit_name,
                                               int trace_node_size,
                                               const std::vector<int> &p2p);
    
    NativeCodeFragment *CreateNativeCodeFragment(NativeCodeFragment *next,
                                                 void **index);

    DISALLOW_IMPLICIT_CONSTRUCTORS(ObjectExtraFactory)
private:
    ManagedAllocator *allocator_;
};

} // namespace mio

#endif // MIO_VM_OBJECT_EXTRA_FACTORY_H_
