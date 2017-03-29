#include "vm.h"
#include "vm-memory-segment.h"
#include "vm-thread.h"
#include "vm-objects.h"

namespace mio {

VM::VM()
    : main_thread_(new Thread(this))
    , code_(new MemorySegment()) {
}

VM::~VM() {
    delete code_;
    delete main_thread_;
}

} // namespace mio
