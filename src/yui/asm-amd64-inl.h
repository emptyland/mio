//
// ! This code from Google V8 !
//

#ifndef YUI_ASM_AMD64_INL_H
#define YUI_ASM_AMD64_INL_H

#define AMD64_MAX_REGARGS    8
#define AMD64_MAX_XMMARGS    8
#define AMD64_MAX_ALLOCREGS 11
#define AMD64_MAX_ALLOCXMMS 15

//
// Arithmetics:
//
// Normal Arithmetics Instruction Mode:
// to        | from      | postfix
// ----------|-----------|---------
// register <- register  : _r_r
// register <- immediate : _r_i
// register <- operand   : _r_op
// operand  <- register  : _op_r
// operand  <- immediate : _op_i
// xmm      <- xmm       : _x_x
// xmm      <- register  : _x_r
// operand  <- xmm       : _x_op
//
// 0x2B, 0x5, 0x2B, 0x29, 0x5

#define ARITH_OP_LIST(_) \
    _(add, \
           0x3, 0x0, 0x3, 0x1, 0x0, \
           0x1, 0x0, 0x3, 0x1, 0x0, \
           0x0, 0x0, 0x2, 0x0, 0x0) \
    _(sub, \
           0x2B, 0x5, 0x2B, 0x29, 0x5, \
           0x29, 0x5, 0x2B, 0x29, 0x5, \
           0x28, 0x5, 0x2A, 0x29, 0x5) \
    _(cmp, \
           0x3B, 0x7, 0x3B, 0x39, 0x7, \
           0x3B, 0x7, 0x3B, 0x39, 0x7, \
           0x3A, 0x7, 0x3A, 0x38, 0x7)

#define DEF_ARITH(name, q_r_r, q_r_i, q_r_o, q_o_r, q_o_i, w_r_r, w_r_i, w_r_o, w_o_r, w_o_i, b_r_r, b_r_i, b_r_o, b_o_r, b_o_i) \
    DEF_ARITH_LONG(name##q,   8, q_r_r, q_r_i, q_r_o, q_o_r, q_o_i) \
    DEF_ARITH_LONG(name##l,   4, q_r_r, q_r_i, q_r_o, q_o_r, q_o_i) \
    DEF_ARITH_SHORT(name##w, 16, w_r_r, w_r_i, w_r_o, w_o_r, w_o_i) \
    DEF_ARITH_SHORT(name##b,  8, b_r_r, b_r_i, b_r_o, b_o_r, b_o_i)

#define DEF_ARITH_LONG(name, size, c_r_r, c_r_i, c_r_o, c_o_r, c_o_i) \
    static inline void Emit_##name##_r_r(Asm *state, Reg dst, Reg src) { \
        EmitArithOp_r_r(state, c_r_r, dst, src, size); \
    } \
    static inline void Emit_##name##_r_i(Asm *state, Reg dst, Imm src) { \
        EmitArithOp_r_i(state, c_r_i, dst, src, size); \
    } \
    static inline void Emit_##name##_r_op(Asm *state, Reg dst, OpdRef src) { \
        EmitArithOp_r_op(state, c_r_o, dst, src, size); \
    } \
    static inline void Emit_##name##_op_r(Asm *state, OpdRef dst, Reg src) { \
        EmitArithOp_r_op(state, c_o_r, src, dst, size); \
    } \
    static inline void Emit_##name##_op_i(Asm *state, OpdRef dst, Imm src) { \
        EmitArithOp_op_i(state, c_o_i, dst, src, size); \
    }

#define DEF_ARITH_SHORT(name, size, c_r_r, c_r_i, c_r_o, c_o_r, c_o_i) \
    static inline void Emit_##name##_r_r(Asm *state, Reg dst, Reg src) { \
        EmitArithOp##size##_r_r(state, c_r_r, dst, src); \
    } \
    static inline void Emit_##name##_r_i(Asm *state, Reg dst, Imm src) { \
        EmitArithOp##size##_r_i(state, c_r_i, dst, src); \
    } \
    static inline void Emit_##name##_r_op(Asm *state, Reg dst, OpdRef src) { \
        EmitArithOp##size##_r_op(state, c_r_o, dst, src); \
    } \
    static inline void Emit_##name##_op_r(Asm *state, OpdRef dst, Reg src) { \
        EmitArithOp##size##_r_op(state, c_o_r, src, dst); \
    } \
    static inline void Emit_##name##_op_i(Asm *state, OpdRef dst, Imm src) { \
        EmitArithOp##size##_op_i(state, c_o_i, dst, src); \
    }

// add
#define Emit_add_r_r(state, dst, src, size) \
    EmitArithOp_r_r(state, 0x03, dst, src, size)

#define Emit_add_r_i(state, dst, src, size) \
    EmitArithOp_r_i(state, 0x0, dst, src, size)

#define Emit_add_r_op(state, dst, src, size) \
    EmitArithOp_r_op(state, 0x03, dst, src, size)

#define Emit_add_op_r(state, dst, src, size) \
    EmitArithOp_r_op(state, 0x1, src, dst, size)

#define Emit_add_op_i(state, dst, src, size) \
    EmitArithOp_op_i(state, 0x0, dst, src, size)

// sub
#define Emit_sub_r_r(state, dst, src, size) \
    EmitArithOp_r_r(state, 0x2B, dst, src, size)

#define Emit_sub_r_i(state, dst, src, size) \
    EmitArithOp_r_i(state, 0x5, dst, src, size)

#define Emit_sub_r_op(state, dst, src, size) \
    EmitArithOp_r_i(state, 0x2B, dst, src, size)

#define Emit_sub_op_r(state, dst, src, size) \
    EmitArithOp_op_r(state, 0x29, dst, src, size)

#define Emit_sub_op_i(state, dst, src, size) \
    EmitArithOp_op_i(state, 0x5, dst, src, size)

// mul

// div

// and

// or

// xor
// 32 bit operations zero the top 32 bits of 64 bit registers.
// there is no need to make this a 64 operation.
#define Emit_xor_r_r(state, dst, src, size) do { \
    if ((size) == sizeof(uint64_t) && (dst).code == (src).code) { \
        EmitArithOp_r_r(state, 0x33, dst, src, sizeof(uint32_t)); \
    } else { \
        EmitArithOp_r_r(state, 0x33, dst, src, size); \
    } \
} while (0)

#define Emit_xor_r_op(state, dst, src, size) \
    EmitArithOp_r_op(state, 0x33, dst, src, size)

#define Emit_xor_r_i(state, dst, src, size) \
    EmitArithOp_r_i(state, 0x6, dst, src, size)

#define Emit_xor_op_i(state, dst, src, size) \
    EmitArithOp_op_i(state, 0x6, dst, src, size)

#define Emit_xor_op_r(state, dst, src, size) \
    EmitArithOp_op_r(state, 0x31, src, dst, size)

// cmp
#define Emit_cmp_r_r(state, dst, src, size) \
    EmitArithOp_r_r(state, 0x3B, dst, src, size)

#define Emit_cmp_r_op(state, dst, src, size) \
    EmitArithOp_r_op(state, 0x3B, dst, src, size)

#define Emit_cmp_op_r(state, dst, src, size) \
    EmitArithOp_op_r(state, 0x39, dst, src, size)

#define Emit_cmp_r_i(state, dst, src, size) \
    EmitArithOp_r_i(state, 0x7, dst, src, size)

#define Emit_cmp_op_i(state, dst, src, size) \
    EmitArithOp_op_i(state, 0x7, dst, src, size)

// shift
#define Emit_rol(state, dst, imm8, size) \
    Emit_shift_r_i(state, dst, imm8, 0x0, size)

#define Emit_rol_cl(state, dst, size) \
    Emit_shift_r(state, dst, 0x0, size)

#define Emit_ror(state, dst, imm8, size) \
    Emit_shift_r_i(state, dst, imm8, 0x1, size)

#define Emit_ror_cl(state, dst, size) \
    Emit_shift_r(state, dst, 0x1, size)

#define Emit_rcl(state, dst, imm8, size) \
    Emit_shift_r_i(state, dst, imm8, 0x2, size)

#define Emit_rcl_cl(state, dst, size) \
    Emit_shift_r(state, dst, 0x2, size)

#define Emit_rcr(state, dst, imm8, size) \
    Emit_shift_r_i(state, dst, imm8, 0x3, size)

#define Emit_rcr_cl(state, dst, size) \
    Emit_shift_r(state, dst, 0x3, size)

#define Emit_shl(state, dst, imm8, size) \
    Emit_shift_r_i(state, dst, imm8, 0x4, size)

#define Emit_shl_cl(state, dst, size) \
    Emit_shift_r(state, dst, 0x4, size)

#define Emit_shr(state, dst, imm8, size) \
    Emit_shift_r_i(state, dst, imm8, 0x5, size)

#define Emit_shr_cl(state, dst, size) \
    Emit_shift_r(state, dst, 0x5, size)

#define Emit_sar(state, dst, imm8, size) \
    Emit_shift_r_i(state, dst, imm8, 0x7, size)

#define Emit_sar_cl(state, dst, size) \
    Emit_shift_r(state, dst, 0x7, size)

// misc

#define Emit_int3(state) EmitB(state, 0xCC)

// misc end

//
// floating instructions:
//

//
// FINIT/FNINIT—Initialize Floating-Point Unit
//
#define Emit_finit(state) do { \
    EmitB(state, 0x9B); \
    EmitB(state, 0xDB); \
    EmitB(state, 0xE3); \
} while (0)

#define Emit_fninit(state) do { \
    EmitB(state, 0xDB); \
    EmitB(state, 0xE3); \
} while (0)

// load data to st(0)
#define Emit_fld(state, i) EmitFArith(state, 0xD9, 0xC0, i)

#define Emit_fld1(state) do { \
    EmitB(0xD9); \
    EmitB(0xE8); \
} while (0)

#define Emit_fldz(state) do { \
    EmitB(0xD9); \
    EmitB(0xEE); \
} while (0)

#define Emit_fldpi(state) do { \
    EmitB(0xD9); \
    EmitB(0xEB); \
} while (0)

#define Emit_fldln2(state) do { \
    EmitB(0xD9); \
    EmitB(0xED); \
} while (0)

// load float and pop
#define Emit_fld_s(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xD9); \
    EmitOperand(0, addr); \
} while (0)

// load double number and pop
#define Emit_fld_d(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xDD); \
    EmitOperand(0, addr); \
} while (0)

// store float number and pop
#define Emit_fstp_s(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xD9); \
    EmitOperand(state, 3, addr); \
} while (0)

// store double number and pop
#define Emit_fstp_d(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(0xDD); \
    EmitOperand(state, 3, addr); \
} while (0)

#define Emit_fstp(state, i) EmitFArith(state, 0xDD, 0xD8, i)

// load dword to st(0)
#define Emit_fild_s(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xDB); \
    EmitOperand_op(state, 0, addr); \
} while (0)

// load qword to st(0)
#define Emit_fild_d(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xDF); \
    EmitOperand_op(state, 5, addr); \
} while (0)


//
// FIST Store Integer
//
#define Emit_fist_s(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xDB); \
    EmitOperand_op(state, 2, addr); \
} while (0)

//
// FISTP Store Integer and Pop
//
// store st(0) to dword and pop
#define Emit_fistp_s(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xDB); \
    EmitOperand_op(state, 3, addr); \
} while (0)

// store st(0) to qword and pop
#define Emit_fistp_d(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xDF); \
    EmitOperand_op(state, 7, addr); \
} while (0)

//
// FISTTP Store Integer with Truncation
// Only SSE3
// store st(0) to dword and pop
#define Emit_fisttp_s(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xDB); \
    EmitOperand_op(state, 1, addr); \
} while (0)

// store st(0) to qword and pop
#define Emit_fisttp_d(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xDD); \
    EmitOperand_op(state, 1, addr); \
} while (0)


//
// ADD
//
// Add m32fp to ST(0) and store result in ST(0).
#define Emit_fadd_s(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xD8); \
    EmitOperand_op(state, 0, addr); \
} while (0)

// Add m64fp to ST(0) and store result in ST(0).
#define Emit_fadd_d(state, addr) do { \
    EmitOptionalRex32_op(state, addr); \
    EmitB(state, 0xDC); \
    EmitOperand_op(state, 0, addr); \
} while (0)

// Add ST(i) to ST(0) and store result in ST(i).
#define Emit_fadd(state, i) EmitFArith(state, 0xDC, 0xC0, i)

// Add ST(0) to ST(1), store result in ST(1), and pop the register stack.
#define Emit_faddp(state) do { \
    EmitB(state, 0xDE); \
    EmitB(state, 0xC1); \
} while (0)


//
// SSE
//
// ADDSS—Add Scalar Single-Precision Floating-Point Values
#define Emit_addss_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0xF3, 0x58, dst, src)

#define Emit_addss_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0xF3, 0x58, dst, src)

// SUBSS—Subtract Scalar Single-Precision Floating-Point Values
#define Emit_subss_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0xF3, 0x5C, dst, src)

#define Emit_subss_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0xF3, 0x5C, dst, src)

// MULSS—Multiply Scalar Single-Precision Floating-Point Values
#define Emit_mulss_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0xF3, 0x59, dst, src)

#define Emit_mulss_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0xF3, 0x59, dst, src)

// DIVSS—Divide Scalar Single-Precision Floating-Point Values
#define Emit_divss_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0xF3, 0x5E, dst, src)

#define Emit_divss_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0xF3, 0x5E, dst, src)

// CVTTSS2SI—Convert with Truncation Scalar Single-Precision FP Value to Dword Integer
#define Emit_cvttss2si_r_x(state, dst, src, size) \
    EmitSSEArith_r_x(state, 0xF3, 0x2C, dst, src, size)

#define Emit_cvttss2si_r_op(state, dst, src, size) \
    EmitSSEArith_r_op(state, 0xF3, 0x2C, dst, src, size)

// ANDPS—Bitwise Logical AND of Packed Single-Precision Floating-Point Values
#define Emit_andps_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0, 0x54, dst ,src)

#define Emit_andps_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0, 0x54, dst, src)

// ORPS—Bitwise Logical OR of Single-Precision Floating-Point Values
#define Emit_orps_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0, 0x56, dst, src)

#define Emit_orps_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0, 0x56, dst, src)

// XORPS—Bitwise Logical XOR for Single-Precision Floating-Point Values
#define Emit_xorps_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0, 0x57, dst, src)

#define Emit_xorps_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0, 0x57, dst, src)

// ADDPS—Add Packed Single-Precision Floating-Point Values
#define Emit_addps_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0, 0x58, dst, src)

#define Emit_addps_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0, 0x58, dst, src)

// SUBPS—Subtract Packed Single-Precision Floating-Point Values
#define Emit_subps_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0, 0x5C, dst, src)

#define Emit_subps_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0, 0x5C, dst, src)

// MULPS—Multiply Packed Single-Precision Floating-Point Values
#define Emit_mulps_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0, 0x59, dst, src)

#define Emit_mulps_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0, 0x59, dst, src)

// DIVPS—Divide Packed Single-Precision Floating-Point Values
#define Emit_divps_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0, 0x5E, dst, src)

#define Emit_divps_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0, 0x5E, dst, src)

// UCOMISS—Unordered Compare Scalar Single-Precision Floating-Point Values and Set EFLAGS
#define Emit_ucomiss_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0, 0x2E, dst, src)

#define Emit_ucomiss_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0, 0x2E, dst, src)

//
// SSE2
//
// ADDSD—Add Scalar Double-Precision Floating-Point Values
#define Emit_addsd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0xF2, 0x58, dst, src)

#define Emit_addsd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0xF2, 0x58, dst, src)

// SUBSD—Subtract Scalar Double-Precision Floating-Point Values
#define Emit_subsd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0xF2, 0x5C, dst, src)

#define Emit_subsd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0xF2, 0x5C, dst, src)

// MULSD—Multiply Scalar Double-Precision Floating-Point Values
#define Emit_mulsd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0xF2, 0x59, dst, src)

#define Emit_mulsd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0xF2, 0x59, dst, src)

// DIVSD—Divide Scalar Double-Precision Floating-Point Values
#define Emit_divsd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0xF2, 0x5E, dst, src)

#define Emit_divsd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0xF2, 0x5E, dst, src)

// CVTTSD2SI—Convert with Truncation Scalar Double-Precision FP Value to Signed Integer
#define Emit_cvttsd2si_r_x(state, dst, src, size) \
    EmitSSEArith_r_x(state, 0xF2, 0x2C, dst, src, size)

#define Emit_cvttsd2si_r_op(state, dst, src, size) \
    EmitSSEArith_r_op(state, 0xF2, 0x2C, dst, src, size)

// ANDPD—Bitwise Logical AND of Packed Double-Precision Floating-Point Values
#define Emit_andpd_x_x(state, dst, src) \
    EmitSSEArtih_x_x(state, 0x66, 0x54, dst, src)

#define Emit_andpd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0x66, 0x54, dst, src)

// ORPD—Bitwise Logical OR of Double-Precision Floating-Point Values
#define Emit_orpd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0x66, 0x56, dst, src)

#define Emit_orpd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0x66, 0x56, dst, src)

// XORPD—Bitwise Logical XOR for Double-Precision Floating-Point Values
#define Emit_xorpd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0x66, 0x57, dst, src)

#define Emit_xorpd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0x66, 0x57, dst, src)

// ADDPD—Add Packed Double-Precision Floating-Point Values
#define Emit_addpd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0x66, 0x58, dst, src)

#define Emit_addpd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0x66, 0x58, dst, src)

// SUBPD—Subtract Packed Double-Precision Floating-Point Values
#define Emit_subpd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0x66, 0x5C, dst, src)

#define Emit_subpd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0x66, 0x5C, dst, src)

// MULPD—Multiply Packed Double-Precision Floating-Point Values
#define Emit_mulpd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0x66, 0x59, dst, src)

#define Emit_mulpd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0x66, 0x59, dst, src)

// DIVPD—Divide Packed Double-Precision Floating-Point Values
#define Emit_divpd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0x66, 0x5E, dst, src)

#define Emit_divpd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0x66, 0x5E, dst, src)

// UCOMISD—Unordered Compare Scalar Double-Precision Floating-Point Values and Set EFLAGS
#define Emit_ucomisd_x_x(state, dst, src) \
    EmitSSEArith_x_x(state, 0x66, 0x2E, dst, src)

#define Emit_ucomisd_x_op(state, dst, src) \
    EmitSSEArith_x_op(state, 0x66, 0x2E, dst, src)


#define EmitTo(state, type, x) do { \
    *((type *)(state)->pc) = (type)(x); \
    (state)->pc += sizeof(type); \
} while (0)

#define EmitB(state, x)  EmitTo(state, uint8_t, x)
#define EmitW(state, x)  EmitTo(state, uint16_t, x)
#define EmitDW(state, x) EmitTo(state, uint32_t, x)
#define EmitQW(state, x) EmitTo(state, uint64_t, x)
#define EmitP0(state, x) EmitTo(state, uintptr_t, x)

#define RegBits(reg)   (1 << (reg).code)
#define RegHiBit(reg)  ((reg).code >> 3)
#define RegLoBits(reg) ((reg).code & 0x7)
#define RegIsByte(reg) ((reg).code <= 3)

#define XmmHiBit(reg)  RegHiBit(reg)
#define XmmLoBits(reg) RegLoBits(reg)

#define EmitRex2(state, arg_mod, arg1, arg2, size) do { \
    if ((size) == sizeof(uint64_t)) { \
        EmitRex64_##arg_mod(state, arg1, arg2); \
    } else { \
        assert((size) == sizeof(uint32_t)); \
        EmitOptionalRex32_##arg_mod(state, arg1, arg2); \
    } \
} while (0)

#define EmitRex1(state, arg_mod, arg, size) do { \
    if ((size) == sizeof(uint64_t)) { \
        EmitRex64_##arg_mod(state, arg); \
    } else { \
        assert((size) == sizeof(uint32_t)); \
        EmitOptionalRex32_##arg_mod(state, arg); \
    } \
} while (0)

#define EmitRex64(state) \
    EmitB(state, 0x48)

#define EmitRex64_r_r(state, reg, rm_reg) \
    EmitB(state, 0x48 | RegHiBit(reg) << 2 | RegHiBit(rm_reg))

#define EmitRex64_x_x(state, reg, rm_reg) \
    EmitB(state, 0x48 | ((reg).code & 0x8) >> 1 | (rm_reg).code >> 3)

#define EmitRex64_x_r(state, reg, rm_reg) EmitRex64_x_x(state, reg, rm_reg)

#define EmitRex64_r_x(state, reg, rm_reg) EmitRex64_x_x(state, reg, rm_reg)

#define EmitRex64_r_op(state, reg, opd) \
    EmitB(state, 0x48 | RegHiBit(reg) << 2 | (opd)->rex)

#define EmitRex64_x_op(state, reg, opd) \
    EmitB(state, 0x48 | ((reg).code & 0x8) >> 1 | (opd)->rex)

#define EmitRex64_r(state, rm_reg) do { \
    assert(((rm_reg).code & 0xf) == (rm_reg).code); \
    EmitB(state, 0x48 | RegHiBit(rm_reg)); \
} while (0)

#define EmitRex64_op(state, opd) \
    EmitB(state, 0x48 | (opd)->rex)

#define EmitRex32_r_r(state, reg, rm_reg) \
    EmitB(state, 0x40 | RegHiBit(reg) << 2 | RegHiBit(rm_reg))

#define EmitRex32_r_op(state, reg, opd) \
    EmitB(state, 0x40 | RegHiBit(reg) << 2 | (opd)->rex)

#define EmitRex32_r(state, rm_reg) \
    EmitB(state, 0x40 | RegHiBit(rm_reg))

#define EmitRex32_op(state, opd) \
    EmitB(state, 0x40 | (opd)->rex)

#define EmitOptionalRex32_r_r(state, reg, rm_reg) do { \
    uint8_t __rex_bits = RegHiBit(reg) << 2 | RegHiBit(rm_reg); \
    if (__rex_bits != 0) \
        EmitB(state, 0x40 | __rex_bits); \
} while (0)

#define EmitOptionalRex32_r_op(state, reg, opd) do { \
    uint8_t __rex_bits = RegHiBit(reg) << 2 | (opd)->rex; \
    if (__rex_bits != 0) \
        EmitB(state, 0x40 | __rex_bits); \
} while (0)

#define EmitOptionalRex32_x_op(state, reg, opd) do { \
    uint8_t __rex_bits = ((reg).code & 0x8) >> 1 | (opd)->rex; \
    if (__rex_bits != 0) \
        EmitB(state, 0x40 | __rex_bits); \
} while (0)

#define EmitOptionalRex32_x_x(state, reg, base) do { \
    uint8_t __rex_bits = ((reg).code & 0x8) >> 1 | ((base).code & 0x8) >> 3; \
    if (__rex_bits != 0) \
        EmitB(state, 0x40 | __rex_bits); \
} while (0)

#define EmitOptionalRex32_x_r(state, reg, base) \
    EmitOptionalRex32_x_x(state, reg, base)

#define EmitOptionalRex32_r_x(state, reg, base) \
    EmitOptionalRex32_x_x(state, reg, base)

#define EmitOptionalRex32_r(state, rm_reg) do { \
    if (RegHiBit(rm_reg)) \
        EmitB(state, 0x41); \
} while (0)

#define EmitOptionalRex32_op(state, opd) do { \
    if ((opd)->rex != 0) \
        EmitB(state, 0x40 | (opd)->rex); \
} while (0)

#define EmitModRM(state, reg, rm_reg) \
    EmitB(state, 0xc0 | RegLoBits(reg) << 3 | RegLoBits(rm_reg))

#define EmitModRM0(state, n, rm_reg) do { \
    assert(IsUintN((n), 3)); \
    EmitB(state, 0xc0 | ((n) << 3) | RegLoBits(rm_reg)); \
} while (0)

#define EmitOperand_r_op(state, reg, opd) \
    EmitOperand(state, RegLoBits(reg), opd)

#define EmitOperand_x_op(state, reg, opd) do { \
    Reg __copied = { (reg).code }; \
    EmitOperand_r_op(state, __copied, opd); \
} while (0)

#define EmitOperand_x_x(state, dst, src) \
    EmitB(state, 0xC0 | (XmmLoBits(dst) << 3) | XmmLoBits(src))

#define EmitOperand_x_r(state, dst, src) \
    EmitB(state, 0xC0 | (XmmLoBits(dst) << 3) | RegLoBits(src))

#define EmitOperand_r_x(state, dst, src) \
    EmitB(state, 0xC0 | (RegLoBits(dst) << 3) | XmmLoBits(src))

#define EmitFArith(state, b1, b2, i) do { \
    assert(IsUint(b1, 8) && IsUintN(b2, 8)); \
    assert(IsUint3(i)); \
    EmitB(state, b1); \
    EmitB(state, (b2) + (i)); \
} while (0)

#define Bind(state, l) BindTo(state, l, PCOffset(state))

#define PCOffset(state) ((int)((state)->pc - (state)->code))

#endif // YUI_ASM_AMD64_INL_H
