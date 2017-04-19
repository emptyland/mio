#ifndef VM_GARBAGE_COLLECTOR_H_
#define VM_GARBAGE_COLLECTOR_H_

#include "vm-object-factory.h"

namespace mio {

class GarbageCollector : public ObjectFactory {
public:
    GarbageCollector() = default;
    virtual ~GarbageCollector() = default;

    /** 
     * GC run one step
     *
     * @param tick -1 force gc, otherwise tick.
     */
    virtual void Step(int tick) = 0;

    virtual void WriteBarrier(HeapObject *target, HeapObject *other) = 0;

    virtual void FullGC() = 0;

    virtual void Active(bool pause) = 0;

    DISALLOW_IMPLICIT_CONSTRUCTORS(GarbageCollector)
};

} // namespace mio

#endif // VM_GARBAGE_COLLECTOR_H_
