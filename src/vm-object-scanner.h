#ifndef VM_OBJECT_SCANNER_H_
#define VM_OBJECT_SCANNER_H_

#include "base.h"
#include <functional>
#include <unordered_set>

namespace mio {

class HeapObject;

class ObjectScanner {
public:
    typedef std::function<void (HeapObject *)> Callback;

    ObjectScanner() = default;
    ~ObjectScanner() = default;

    void Scan(HeapObject *ob, Callback callback);

    DISALLOW_IMPLICIT_CONSTRUCTORS(ObjectScanner)
private:
    std::unordered_set<HeapObject *> traced_objects_;
};

} // namespace mio

#endif // VM_OBJECT_SCANNER_H_
