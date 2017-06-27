#ifndef MIO_VM_BITCODE_H_
#define MIO_VM_BITCODE_H_

#include <stdint.h>

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
    M(logic_not) \
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
    M(sub_f64) \
    M(mul_i8) \
    M(mul_i16) \
    M(mul_i32) \
    M(mul_i64) \
    M(mul_f32) \
    M(mul_f64) \
    M(div_i8) \
    M(div_i16) \
    M(div_i32) \
    M(div_i64) \
    M(div_f32) \
    M(div_f64)

#define VM_BIT_BC(M) \
    M(or_i8)         \
    M(or_i16)        \
    M(or_i32)        \
    M(or_i64)        \
    M(xor_i8)        \
    M(xor_i16)       \
    M(xor_i32)       \
    M(xor_i64)       \
    M(and_i8)        \
    M(and_i16)       \
    M(and_i32)       \
    M(and_i64)       \
    M(inv_i8)        \
    M(inv_i16)       \
    M(inv_i32)       \
    M(inv_i64)       \
    M(shl_i8)        \
    M(shl_i16)       \
    M(shl_i32)       \
    M(shl_i64)       \
    M(shl_i8_imm)    \
    M(shl_i16_imm)   \
    M(shl_i32_imm)   \
    M(shl_i64_imm)   \
    M(shr_i8)        \
    M(shr_i16)       \
    M(shr_i32)       \
    M(shr_i64)       \
    M(shr_i8_imm)    \
    M(shr_i16_imm)   \
    M(shr_i32_imm)   \
    M(shr_i64_imm)   \
    M(ushr_i8)       \
    M(ushr_i16)      \
    M(ushr_i32)      \
    M(ushr_i64)      \
    M(ushr_i8_imm)   \
    M(ushr_i16_imm)  \
    M(ushr_i32_imm)  \
    M(ushr_i64_imm)

#define VM_TYPE_CAST(M) \
    M(sext_i32) \
    M(sext_i16) \
    M(sext_i8) \
    M(trunc_i16) \
    M(trunc_i32) \
    M(trunc_i64) \
    M(fptrunc_f32) \
    M(fptrunc_f64) \
    M(fpext_f32) \
    M(fpext_f64) \
    M(fptosi_f32) \
    M(fptosi_f64) \
    M(sitofp_i8) \
    M(sitofp_i16) \
    M(sitofp_i32) \
    M(sitofp_i64)

#define VM_CONTROL_BC(M) \
    M(test) \
    M(jz) \
    M(jnz) \
    M(jmp) \
    M(loop_entry) \

#define VM_CALL_BC(M) \
    M(call) \
    M(call_val) \
    M(frame) \
    M(ret) \
    M(oop) \
    M(close_fn)

#define VM_ALL_BITCODE(M) \
    M(debug)              \
    VM_LOAD_BC(M)         \
    VM_MOV_BC(M)          \
    VM_BIT_BC(M)          \
    VM_ARITH_BC(M)        \
    VM_TYPE_CAST(M)       \
    VM_CONTROL_BC(M)      \
    VM_CALL_BC(M)


#define VM_COMPARATOR(M) \
    M(EQ, ==) \
    M(NE, !=) \
    M(LT, < ) \
    M(LE, <=) \
    M(GT, > ) \
    M(GE, >=)

#define OBJECT_OPERATOR(M) \
    M(UnionOrMerge) \
    M(UnionTest) \
    M(UnionUnbox) \
    M(Array) \
    M(ArraySet) \
    M(ArrayDirectSet) \
    M(ArrayAdd) \
    M(ArrayGet) \
    M(ArraySize) \
    M(Slice) \
    M(Map) \
    M(MapWeak) \
    M(MapPut) \
    M(MapDelete) \
    M(MapGet) \
    M(MapFirstKey) \
    M(MapNextKey) \
    M(MapSize) \
    M(ToString) \
    M(StrCat) \
    M(StrLen)

enum BCInstruction : uint8_t {
#define BitCode_ENUM_DEFINE(name) BC_##name,
    VM_ALL_BITCODE(BitCode_ENUM_DEFINE)
#undef  BitCode_ENUM_DEFINE
    MAX_BC_INSTRUCTIONS,
};

static_assert(MAX_BC_INSTRUCTIONS <= 255, "instructions is too much!");

enum BCSegment : int {
    BC_GLOBAL_PRIMITIVE_SEGMENT,
    BC_GLOBAL_OBJECT_SEGMENT,
    BC_FUNCTION_CONSTANT_PRIMITIVE_SEGMENT,
    BC_FUNCTION_CONSTANT_OBJECT_SEGMENT,
    BC_UP_PRIMITIVE_SEGMENT,
    BC_UP_OBJECT_SEGMENT,
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
 *    * val2:   Index of type info.
 *
 * OO_UnionTest
 * -- desc: Test a union object type.
 * -- params:
 *    * result: Result of testing, non-zero: union is this type, zero: not.
 *    * val1:   Offset of union object for testing.
 *    * val2:   Index of type info for testing.
 *
 * OO_UnionUnbox
 * -- desc: Unbox a union object.
 * -- params:
 *    * result: Result of unboxing.
 *    * val1:   Offset of union object for unboxing.
 *    * val2:   Index of type info for unboxing.
 *
 * OO_Array
 * -- desc: Create a new array object.
 * -- params:
 *    * result: Offset of created array.
 *    * val1:   Index of type info in element.
 *    * val2:   Immediately initial size number of array.
 *
 * OO_ArrayDirectSet
 * -- desc: Set element in array object.
 * -- params:
 *    * result: Offset of array.
 *    * val1:   Immediately index number for setting.
 *    * val2:   Offset of element for setting.
 *
 * OO_ArrayGet
 * -- desc: Get element from array object.
 * -- params:
 *    * result: Offset of array.
 *    * val1:   Offset of index for getting.
 *    * val2:   Offset of element for getting.
 *
 * OO_ArraySize
 * -- desc: Get element from array object.
 * -- params:
 *    * result: Offset of array.
 *    * val1:   Offset of size result.
 *    * val2:   Unused.
 *
 * OO_Slice
 * -- desc: Make slice from array or slice.
 * -- params:
 *    * result: Offset of array.
 *    * val1:   Offset of begin postion for slice.
 *    * val2:   Offset of size for slice.
 *
 * OO_Slice
 * -- desc: Make slice from slice or array object.
 * -- params:
 *    * result: Offset of array or slice object.
 *    * val1:   Offset of begin for slice.
 *    * val2:   Offset of size for slice.
 *
 * OO_Map
 * -- desc: Create a new map object.
 * -- params:
 *    * result: Offset of created map.
 *    * val1:   Index of type info in key.
 *    * val2:   Index of type info in value.
 *
 * OO_MapPut
 * -- desc: Put a key and value pair into map object.
 * -- params:
 *    * result: Offset of map for putting.
 *    * val1:   Key for putting.
 *    * val2:   Value for putting.
 *
 * OO_MapDelete
 * -- desc: Delete key from map object.
 * -- params:
 *    * result: Offset of map for deleting.
 *    * val1:   Key for deleting.
 *    * val2:   Deleting result, 0 key not exists, otherwise key is exists.
 *
 * OO_MapGet
 * -- desc: Get value by key, if not exists, return error.
 *          return type is [`key-type', error].
 * -- params:
 *    * result: Offset of map for getting.
 *    * val1:   Offset of key.
 *    * val2:   Offset of return union object.
 *
 * OO_MapFirstKey
 * -- desc: Get first key and value of map, if has first, pc + 1.
 * -- params:
 *    * result: Offset of map for iteration.
 *    * val1:   Offset of first key.
 *    * val2:   Offset of first value.
 *
 * OO_MapNextKey
 * -- desc: Get next key and value of map, if has next, pc + 1.
 * -- params:
 *    * result: Offset of map for iteration.
 *    * val1:   Offset of input and output key.
 *    * val2:   Offset of value.
 *
 * OO_MapSize
 * -- desc: Get map size.
 * -- params:
 *    * result: Offset of map.
 *    * val1:   Unused.
 *    * val2:   Offset of result for getting size.
 *
 * OO_ToString
 * -- desc: Make a value to string object
 *    * result: Offset of string result.
 *    * val1:   Offset of input.
 *    * val2:   Index of type info.
 *
 * OO_StrCat
 * -- desc: Connect 2 string objects.
 *    * result: Offset of string result be connected.
 *    * val1:   Offset of first one string for connection.
 *    * val2:   Offset of last one string for connection.
 *
 * OO_StrLen
 * -- desc: Get string object payload size.
 *    * result: Offset of string.
 *    * val1:   Unused.
 *    * val2:   Offset of result for getting size.
 */

enum BCObjectOperatorId : int {
#define ObjectOperator_ENUM_DEFINE(name) OO_##name,
    OBJECT_OPERATOR(ObjectOperator_ENUM_DEFINE)
#undef  ObjectOperator_ENUM_DEFINE
    MAX_OO_OPERATORS,
};

enum BCComparator : int {
#define Comparator_ENUM_DEFINE(name, op) CC_##name,
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
extern const char * const kSegmentText[MAX_BC_SEGMENTS];

} // namespace mio

#endif // MIO_VM_BITCODE_H_
