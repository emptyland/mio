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
#define ComparatorText_ELEM_DEFINE(name, op) #name,
    VM_COMPARATOR(ComparatorText_ELEM_DEFINE)
#undef ComparatorText_ELEM_DEFINE
};

const char * const kSegmentText[MAX_BC_SEGMENTS] = {
    "gp", // BC_GLOBAL_PRIMITIVE_SEGMENT,
    "go", // BC_GLOBAL_OBJECT_SEGMENT,
    "cp", // BC_FUNCTION_CONSTANT_PRIMITIVE_SEGMENT,
    "co", // BC_FUNCTION_CONSTANT_OBJECT_SEGMENT,
    "up", // BC_UP_PRIMITIVE_SEGMENT,
    "uo", // BC_UP_OBJECT_SEGMENT,
    "lp", // BC_LOCAL_PRIMITIVE_SEGMENT,
    "lo", // BC_LOCAL_OBJECT_SEGMENT,
};

} // namespace mio
