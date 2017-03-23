#ifndef MIO_VM_BITCODE_H_
#define MIO_VM_BITCODE_H_

#include <stdint.h>

/*
 load(8bits) result(12bits) constant(24bits)

 load_i1
 load_i8
 load_i32
 load_i64
 load_i32_imm
 load_f32
 load_f64

 add(8bits) result(12bits) lhs(12bits) rhs(32bits)

 add_i8
 add_i16
 add_i32
 add_i64
 add_i8_imm
 add_i16_imm
 add_i32_imm
 
 test(8bits) cond(12bits) then(12bits) else(32bits)

 test
 
 call(8bits) function(12bits) base_stack(12bits)
 
 call
 call_native

 */

namespace mio {

#define VM_LOAD_BC(M) \
    M(load_1b) \
    M(load_2b) \
    M(load_4b) \
    M(load_8b) \
    M(load_i32_imm) \
    M(load_o) \

#define VM_MOV_BC(M) \
    M(mov_1b) \
    M(mov_2b) \
    M(mov_4b) \
    M(mov_8b) \
    M(mov_o)

#define VM_ADD_BC(M) \
    M(add_i8) \
    M(add_i16) \
    M(add_i32) \
    M(add_i64) \
    M(add_i8_imm) \
    M(add_i16_imm) \
    M(add_i32_imm) \

#define VM_TEST_BC(M) \
    M(test) \

#define VM_CALL_BC(M) \
    M(call) \
    M(call_val) \
    M(frame) \
    M(ret)

#define VM_ALL_BITCODE(M) \
    M(debug) \
    VM_LOAD_BC(M) \
    VM_MOV_BC(M) \
    VM_ADD_BC(M) \
    VM_TEST_BC(M) \
    VM_CALL_BC(M)


enum BCInstruction : uint8_t {
#define BitCode_ENUM_DEFINE(name) BC_##name,
    VM_ALL_BITCODE(BitCode_ENUM_DEFINE)
#undef  BitCode_ENUM_DEFINE
    MAX_BC_INSTRUCTIONS,
};

enum BCSegment : int {
    BC_CONSTANT_SEGMENT,
    BC_GLOBAL_PRIMITIVE_SEGMENT,
    BC_GLOBAL_OBJECT_SEGMENT,
    BC_LOCAL_PRIMITIVE_SEGMENT,
    BC_LOCAL_OBJECT_SEGMENT,
    BC_FUNCTION,
};

static_assert(MAX_BC_INSTRUCTIONS <= 255, "bitcode is too much!");

struct InstructionMetadata {
    BCInstruction cmd;
    const char *  text;
};

extern const InstructionMetadata kInstructionMetadata[MAX_BC_INSTRUCTIONS];

} // namespace mio

#endif // MIO_VM_BITCODE_H_
