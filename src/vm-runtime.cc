#include "vm-runtime.h"

namespace mio {

const RtNativeFunctionEntry kRtNaFn[] = {
    // base library
    { "::base::print",  &NativeBaseLibrary::Print,  },
    { "::base::tick",   &NativeBaseLibrary::Tick,   },
    { "::base::gc",     &NativeBaseLibrary::GC,     },
    { "::base::fullGC", &NativeBaseLibrary::FullGC, },

    { .name = nullptr, .pointer = nullptr, } // end of functions
};

} // namespace mio
