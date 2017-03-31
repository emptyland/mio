#include "vm-bitcode.h"

namespace mio {

const InstructionMetadata kInstructionMetadata[MAX_BC_INSTRUCTIONS] = {
#define InstructionMetadata_ELEM_DEFINE(name) { .cmd = BC_##name, .text = #name },
    VM_ALL_BITCODE(InstructionMetadata_ELEM_DEFINE)
#undef  InstructionMetadata_ELEM_DEFINE
};

const char * const kObjectOperatorText[MAX_OO_OPERATORS] = {
#define ObjectOperatorText_ELEM_DEFINE(name) #name,
    OBJECT_OPERATOR(ObjectOperatorText_ELEM_DEFINE)
#undef  ObjectOperatorText_ELEM_DEFINE
};

const char * const kComparatorText[MAX_CC_COMPARATORS] = {
#define ComparatorText_ELEM_DEFINE(name) #name,
    VM_COMPARATOR(ComparatorText_ELEM_DEFINE)
#undef ComparatorText_ELEM_DEFINE
};

} // namespace mio
