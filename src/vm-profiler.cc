#include "vm-profiler.h"
#include "vm.h"
#include "vm-thread.h"

namespace mio {

Profiler::Profiler() {
}

Profiler::~Profiler() {
    Stop();
}

void Profiler::SampleTick() {

}

} // namespace mio
