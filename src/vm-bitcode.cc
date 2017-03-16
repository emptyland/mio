#include "vm-bitcode.h"

namespace mio {

const InstructionMetadata kInstructionMetadata[MAX_BC_INSTRUCTIONS] = {
#define InstructionMetadata_ELEM_DEFINE(name) { .cmd = BC_##name, .text = #name },
    VM_ALL_BITCODE(InstructionMetadata_ELEM_DEFINE)
#undef  InstructionMetadata_ELEM_DEFINE
};

} // namespace mio
