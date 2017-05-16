//
// ! This code from Google v8 !
//

#ifndef YUI_AMD64_ASM_AMD64_H
#define YUI_AMD64_ASM_AMD64_H

#include "asm-amd64-inl.h"
#include "asm.h"
#include <stdint.h>
#include <stddef.h>

// Function postfix:
//
// sign | content          | C type
// -----|------------------|--------
// r    : register         : Reg
// x    | XMM regsiter     : Xmm
// op   : operand          : OpdRef
// i    : immediate number : Imm
// p    : native pointer   | void *
// i32  : native integer   : int32_t
// u32  : native integer   : unt32_t
// i64  : native integer   : int64_t
// u64  : native integer   : unt64_t

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct Reg Reg;
typedef struct Xmm Xmm;
typedef struct Asm Asm;
typedef struct Opd Opd;
typedef struct Imm Imm;

typedef const Opd *OpdRef;

struct Reg {
    int code;
};

struct Xmm {
    int code;
};

struct Asm {
    uint8_t *pc;
    uint8_t *code;
    size_t   size;
};

// Operand struct
struct Opd {
    uint8_t rex;
    uint8_t buf[6];
    uint8_t len;
};

// Machine Immediate
struct Imm {
    int32_t value;
};

typedef enum ScaleFactor {
    times_1 = 0,
    times_2 = 1,
    times_4 = 2,
    times_8 = 3,
    times_int_size = times_4,
    times_ptr_size = times_8,
} ScaleFactor;

typedef enum RegCode {
    kRAX = 0,
    kRCX,
    kRDX,
    kRBX,
    kRSP, // 4

    kRBP, // 5
    kRSI,
    kRDI,
    kR8,
    kR9,  // 9

    kR10, // 10
    kR11,
    kR12,
    kR13,
    kR14, // 14

    kR15,
} RegCode;

typedef enum Cond {
    NoCond       = -1,

    Overflow     = 0,
    NoOverflow   = 1,
    Below        = 2,
    AboveEqual   = 3,
    Equal        = 4,
    NotEqual     = 5,
    BelowEqual   = 6,
    Above        = 7,
    Negative     = 8,
    Positive     = 9,
    ParityEven   = 10,
    ParityOdd    = 11,
    Less         = 12,
    GreaterEqual = 13,
    LessEqual    = 14,
    Greater      = 15,

    // Fake conditions
    Always       = 16,
    Never        = 17,

    Carry    = Below,
    NotCarry = AboveEqual,
    Zero     = Equal,
    NotZero  = NotEqual,
    Sign     = Negative,
    NotSign  = Positive,
    LastCond = Greater,
} Cond;

extern const Reg rax; // 0
extern const Reg rcx; // 1
extern const Reg rdx; // 2
extern const Reg rbx; // 3
extern const Reg rsp; // 4

extern const Reg rbp; // 5
extern const Reg rsi; // 6
extern const Reg rdi; // 7
extern const Reg r8;  // 8
extern const Reg r9;  // 9

extern const Reg r10; // 10
extern const Reg r11; // 11
extern const Reg r12; // 12
extern const Reg r13; // 13
extern const Reg r14; // 14

extern const Reg r15; // 15
extern const Reg rNone; // -1

extern const Reg RegArgv[AMD64_MAX_REGARGS];
extern const Xmm XmmArgv[AMD64_MAX_XMMARGS];
extern const Reg RegAlloc[AMD64_MAX_ALLOCREGS];

//
// The XMM Registers:
//
extern const Xmm xmm0; // 0
extern const Xmm xmm1; // 1
extern const Xmm xmm2; // 2
extern const Xmm xmm3; // 3
extern const Xmm xmm4; // 4

extern const Xmm xmm5; // 5
extern const Xmm xmm6; // 6
extern const Xmm xmm7; // 7
extern const Xmm xmm8; // 8
extern const Xmm xmm9; // 9

extern const Xmm xmm10; // 10
extern const Xmm xmm11; // 11
extern const Xmm xmm12; // 12
extern const Xmm xmm13; // 13
extern const Xmm xmm14; // 14

extern const Xmm xmm15; // 15

// [base + disp/r]
OpdRef Operand0(Opd *opd, Reg base, int32_t disp);

// [base + index * scale + disp/r]
OpdRef Operand1(Opd *opd, Reg base, Reg index, ScaleFactor scale,
                    int32_t disp);

// [index * scale + disp/r]
OpdRef Operand2(Opd *opd, Reg index, ScaleFactor scale, int32_t disp);

// Load Effective Address
void Emit_lea(Asm *state, Reg dst, OpdRef src, int size);

// Read random
void Emit_rdrand(Asm *state, Reg dst, int size);

// Stack Operating
void Emit_pushq_r(Asm *state, Reg src);
void Emit_pushq_i32(Asm *state, int32_t val);
void Emit_pushfq(Asm *state);

void Emit_popq_r(Asm *state, Reg dst);
void Emit_popq_op(Asm *state, OpdRef dst);
void Emit_popfq(Asm *state);

// Move
void Emit_movq_i64(Asm *state, Reg dst, int64_t val);
void Emit_movq_p(Asm *state, Reg dst, void *val);
void Emit_movq_r_r(Asm *state, Reg dst, Reg src, int size);
void Emit_movq_r_op(Asm *state, Reg dst, OpdRef opd, int size);
void Emit_movq_op_r(Asm *state, OpdRef dst, Reg src, int size);
void Emit_movq_r_i(Asm *state, Reg dst, Imm src, int size);
void Emit_movq_op_i(Asm *state, OpdRef dst, Imm src, int size);

void Emit_movb_r_r(Asm *state, Reg dst, Reg src);
void Emit_movb_r_op(Asm *state, Reg dst, OpdRef src);
void Emit_movb_op_r(Asm *state, OpdRef dst, Reg src);
void Emit_movb_r_i(Asm *state, Reg dst, Imm src);
void Emit_movb_op_i(Asm *state, OpdRef dst, Imm src);

void Emit_movw_r_r(Asm *state, Reg dst, Reg src);
void Emit_movw_r_op(Asm *state, Reg dst, OpdRef src);
void Emit_movw_op_r(Asm *state, OpdRef dst, Reg src);
void Emit_movw_r_i(Asm *state, Reg dst, Imm src);
void Emit_movw_op_i(Asm *state, OpdRef dst, Imm src);

// NOTICE: only AH, BH, CH, DH can be extend
// byte -> dword
void Emit_movzxb_r_r(Asm *state, Reg dst, Reg src);
void Emit_movzxb_r_op(Asm *state, Reg dst, OpdRef src);
// word -> dword
void Emit_movzxw_r_r(Asm *state, Reg dst, Reg src);
void Emit_movzxw_r_op(Asm *state, Reg dst, OpdRef src);

// byte -> dword
void Emit_movsxb_r_r(Asm *state, Reg dst, Reg src);
void Emit_movsxb_r_op(Asm *state, Reg dst, OpdRef src);
// word -> dword
void Emit_movsxw_r_r(Asm *state, Reg dst, Reg src);
void Emit_movsxw_r_op(Asm *state, Reg dst, OpdRef src);

// Calling
void Emit_call_l(Asm *state, YILabel *l);
void Emit_call_p(Asm *state, void *p);
void Emit_call_r(Asm *state, Reg addr);
void Emit_call_op(Asm *state, OpdRef opd);

void Emit_ret_i(Asm *state, int val);

// Jumping
void Emit_jmp_l(Asm *state, YILabel *l, int is_far);

// Condition Jumping (jcc)
void Emit_jcc_l(Asm *state, Cond cc, YILabel *l, int is_far);
void Emit_jcc_p(Asm *state, Cond cc, void *p);

// Logic Compare
void Emit_test_r_r(Asm *state, Reg dst, Reg src, int size);
void Emit_test_r_i(Asm *state, Reg dst, Imm mask, int size);
void Emit_test_op_r(Asm *state, OpdRef op, Reg reg, int size);
void Emit_test_op_i(Asm *state, OpdRef op, Imm mask, int size);

// Neg
void Emit_neg_r(Asm *state, Reg dst, int size);
void Emit_neg_op(Asm *state, OpdRef dst, int size);

// Not
void Emit_not_r(Asm *state, Reg dst, int size);
void Emit_not_op(Asm *state, OpdRef dst, int size);

// Shift
void Emit_shift_r_i(Asm *state, Reg dst, Imm amount, int subcode, int size);
void Emit_shift_r(Asm *state, Reg dst, int subcode, int size);

// shift dst:src left by cl bits, affecting only dst.
void Emit_shld(Asm *state, Reg dst, Reg src);

// shift src:dst right by cl bits, affecting only dst.
void Emit_shrd(Asm *state, Reg dst, Reg src);

//
// SSE
//
// MOVAPS—Move Aligned Packed Single-Precision Floating-Point Values
void Emit_movaps_x_x(Asm *state, Xmm dst, Xmm src);
void Emit_movaps_x_op(Asm *state, Xmm dst, OpdRef src);
void Emit_movaps_op_x(Asm *state, OpdRef dst, Xmm src);

// MOVSS—Move Scalar Single-Precision Floating-Point Values
void Emit_movss_x_op(Asm *state, Xmm dst, OpdRef src);
void Emit_movss_op_x(Asm *state, OpdRef dst, Xmm src);

//
// SSE2
//
// MOVAPD—Move Aligned Packed Double-Precision Floating-Point Values
void Emit_movapd_x_x(Asm *state, Xmm dst, Xmm src);
void Emit_movapd_x_op(Asm *state, Xmm dst, OpdRef src);
void Emit_movapd_op_x(Asm *state, OpdRef dst, Xmm src);

// Move Scalar Double-Precision Floating-Point Value
void Emit_movsd_x_op(Asm *state, Xmm dst, OpdRef src);
void Emit_movsd_op_x(Asm *state, OpdRef dst, Xmm src);


void EmitOperand(Asm *state, int code, OpdRef addr);

void EmitSSEArith_x_x(Asm *state, uint8_t prefix, uint8_t subcode, Xmm dst,
                      Xmm src);

void EmitSSEArith_x_op(Asm *state, uint8_t prefix, uint8_t subcode, Xmm dst,
                       OpdRef src);

void EmitSSEArith_r_x(Asm *state, uint8_t prefix, uint8_t subcode, Reg dst,
                      Xmm src, int size);

void EmitSSEArith_r_op(Asm *state, uint8_t prefix, uint8_t subcode, Reg dst,
                       OpdRef src, int size);


void EmitArithOp_r_op(Asm *state, uint8_t op, Reg reg, OpdRef opd,
                      int size);
void EmitArithOp_r_r(Asm *state, uint8_t op, Reg reg, Reg rm_reg, int size);
void EmitArithOp_r_i(Asm *state, uint8_t subcode, Reg dst, Imm src, int size);
void EmitArithOp_op_i(Asm *state, uint8_t subcode, OpdRef dst, Imm src,
                      int size);

void EmitArithOp8_r_op(Asm *state, uint8_t op, Reg reg, OpdRef opd);
void EmitArithOp8_r_r(Asm *state, uint8_t op, Reg reg, Reg rm_reg);
void EmitArithOp8_r_i(Asm *state, uint8_t subcode, Reg dst, Imm src);
void EmitArithOp8_op_i(Asm *state, uint8_t subcode, OpdRef dst, Imm src);

void EmitArithOp16_r_op(Asm *state, uint8_t op, Reg reg, OpdRef opd);
void EmitArithOp16_r_r(Asm *state, uint8_t op, Reg reg, Reg rm_reg);
void EmitArithOp16_r_i(Asm *state, uint8_t subcode, Reg dst, Imm src);
void EmitArithOp16_op_i(Asm *state, uint8_t subcode, OpdRef dst, Imm src);

//DEF_ARITH_LONG(addq,   8, 0x3, 0x0, 0x3, 0x1, 0x0)
//DEF_ARITH_LONG(addl,   4, 0x3, 0x0, 0x3, 0x1, 0x0)
//DEF_ARITH_SHORT(addw, 16, 0x1, 0x0, 0x3, 0x1, 0x0)
//DEF_ARITH_SHORT(addb,  8, 0x0, 0x0, 0x2, 0x0, 0x0)

ARITH_OP_LIST(DEF_ARITH)

void BindTo(Asm *state, YILabel *l, int pos);

#if defined(__cplusplus)
}
#endif

#endif // YUI_AMD64_ASM_AMD64_H
