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
    M(load_i8_imm) \
    M(load_i16_imm) \
    M(load_i32_imm) \
    M(load_o) \
    M(store_1b) \
    M(store_2b) \
    M(store_4b) \
    M(store_8b) \
    M(store_o)

#define VM_MOV_BC(M) \
    M(mov_1b) \
    M(mov_2b) \
    M(mov_4b) \
    M(mov_8b) \
    M(mov_o)

#define VM_ARITH_BC(M) \
    M(cmp_i8) \
    M(cmp_i16) \
    M(cmp_i32) \
    M(cmp_i64) \
    M(cmp_f32) \
    M(cmp_f64) \
    M(add_i8) \
    M(add_i16) \
    M(add_i32) \
    M(add_i64) \
    M(add_f32) \
    M(add_f64) \
    M(add_i8_imm) \
    M(add_i16_imm) \
    M(add_i32_imm) \
    M(sub_i8) \
    M(sub_i16) \
    M(sub_i32) \
    M(sub_i64) \
    M(sub_f32) \
    M(sub_f64)

#define VM_TEST_BC(M) \
    M(test) \
    M(jz) \
    M(jnz) \
    M(jmp)

#define VM_CALL_BC(M) \
    M(call) \
    M(call_val) \
    M(frame) \
    M(ret) \
    M(oop)

#define VM_ALL_BITCODE(M) \
    M(debug) \
    VM_LOAD_BC(M) \
    VM_MOV_BC(M) \
    VM_ARITH_BC(M) \
    VM_TEST_BC(M) \
    VM_CALL_BC(M)


#define VM_COMPARATOR(M) \
    M(EQ) \
    M(NE) \
    M(LT) \
    M(LE) \
    M(GT) \
    M(GE)

#define OBJECT_OPERATOR(M) \
    M(UnionOrMerge) \
    M(UnionVoid) \
    M(Map) \
    M(MapPut) \
    M(MapGet)

enum BCInstruction : uint8_t {
#define BitCode_ENUM_DEFINE(name) BC_##name,
    VM_ALL_BITCODE(BitCode_ENUM_DEFINE)
#undef  BitCode_ENUM_DEFINE
    MAX_BC_INSTRUCTIONS,
};

static_assert(MAX_BC_INSTRUCTIONS <= 255, "instructions is too much!");

enum BCSegment : int {
    BC_CONSTANT_SEGMENT,
    BC_GLOBAL_PRIMITIVE_SEGMENT,
    BC_GLOBAL_OBJECT_SEGMENT,
    BC_LOCAL_PRIMITIVE_SEGMENT,
    BC_LOCAL_OBJECT_SEGMENT,
    MAX_BC_SEGMENTS,
};

/**
 * Object Operators:
 *
 * OO_UnionOrMerge
 * -- desc: Create or merge union.
 * -- params:
 *    * result: Offset of union object.
 *    * val1:   Value for inbox.
 *    * val2:   Index of value type id.
 *
 * OO_Map
 * -- desc: Create a new map object.
 * -- params:
 *    * result: Offset of created map.
 *    * val1:   Type of kind code(Type::Kind) in key
 *    * val2:   Type of kind code(Type::Kind) in value
 *
 * OO_MapPut
 * -- desc: Put a key and value pair into map object.
 * -- params:
 *    * result: Offset of map for putting.
 *    * val1:   Key for putting.
 *    * val2:   Value for putting.
 *
 * OO_MapGet
 * -- desc: Get value by key, if not exists, return error.
 *          return type is [`key-type', error].
 * -- params:
 *    * result: Offset of map for getting.
 *    * val1:   Offset of key.
 *    * val2:   Offset of return union object.
 */

enum BCObjectOperatorId : int {
#define ObjectOperator_ENUM_DEFINE(name) OO_##name,
    OBJECT_OPERATOR(ObjectOperator_ENUM_DEFINE)
#undef  ObjectOperator_ENUM_DEFINE
    MAX_OO_OPERATORS,
};

enum BCComparator : int {
#define Comparator_ENUM_DEFINE(name) CC_##name,
    VM_COMPARATOR(Comparator_ENUM_DEFINE)
#undef  Comparator_ENUM_DEFINE
    MAX_CC_COMPARATORS,
};

struct InstructionMetadata {
    BCInstruction cmd;
    const char *  text;
};

extern const InstructionMetadata kInstructionMetadata[MAX_BC_INSTRUCTIONS];
extern const char * const kObjectOperatorText[MAX_OO_OPERATORS];
extern const char * const kComparatorText[MAX_CC_COMPARATORS];

} // namespace mio

#endif // MIO_VM_BITCODE_H_
