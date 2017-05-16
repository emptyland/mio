//
// ! This code based from Google v8 !
//

#include "asm-amd64.h"
#include <assert.h>
#include <stdio.h>

const Reg rax = {kRAX};
const Reg rcx = {kRCX};
const Reg rdx = {kRDX};
const Reg rbx = {kRBX};
const Reg rsp = {kRSP};

const Reg rbp = {kRBP};
const Reg rsi = {kRSI};
const Reg rdi = {kRDI};
const Reg r8  = {kR8};
const Reg r9  = {kR9};

const Reg r10 = {kR10};
const Reg r11 = {kR11};
const Reg r12 = {kR12};
const Reg r13 = {kR13};
const Reg r14 = {kR14};

const Reg r15 = {kR15};
const Reg rNone = {-1};

const Reg RegArgv[AMD64_MAX_REGARGS] = {
    {kRDI},
    {kRSI},
    {kRDX},
    {kRCX},
    {kR8},
    {kR9},
    {kR10},
    {kR11},
};

const Xmm XmmArgv[AMD64_MAX_XMMARGS] = {
    {0},
    {1},
    {2},
    {3},
    {4},
    {5},
    {6},
    {7},
};

// The non-allocatable registers are:
// rsp - stack pointer
// rbp - frame pointer
// r10 - fixed scratch register
// r12 - smi constant register
// r13 - root register
//
const Reg RegAlloc[AMD64_MAX_ALLOCREGS] = {
    {kRAX}, // 0
    {kRBX},
    {kRDX},
    {kRCX},
    {kRSI}, // 4
    {kRDI},
    {kR8},
    {kR9},
    {kR11}, // 8
    {kR14},
    {kR15},
};

// 128bit xmm registers:
const Xmm xmm0  = {0}; // 0
const Xmm xmm1  = {1}; // 1
const Xmm xmm2  = {2}; // 2
const Xmm xmm3  = {3}; // 3
const Xmm xmm4  = {4}; // 4

const Xmm xmm5  = {5}; // 5
const Xmm xmm6  = {6}; // 6
const Xmm xmm7  = {7}; // 7
const Xmm xmm8  = {8}; // 8
const Xmm xmm9  = {9}; // 9

const Xmm xmm10 = {10}; // 10
const Xmm xmm11 = {11}; // 11
const Xmm xmm12 = {12}; // 12
const Xmm xmm13 = {13}; // 13
const Xmm xmm14 = {14}; // 14

const Xmm xmm15 = {15}; // 15

static int IsIntN(int64_t x, unsigned n) {
    assert((0 < n) && (n < 64));
    int64_t limit = (int64_t)(1) << (n - 1);
    return (-limit <= x) && (x < limit);
}

static int IsUintN(int64_t x, unsigned n) {
    assert((0 < n) && (n < sizeof(x) * 8));
    return !(x >> n);
}

static void OpdSetModRM(Opd *opd, int mod, Reg rm_reg) {
    assert(IsUintN(mod, 2));
    opd->buf[0] = mod << 6 | RegLoBits(rm_reg);
    opd->rex |= RegHiBit(rm_reg);
}

static void OpdSetSIB(Opd *opd, ScaleFactor scale, Reg index, Reg base) {
    assert(opd->len == 1);
    assert(IsUintN(scale, 2));

    assert(index.code != rsp.code || base.code == rsp.code ||
           base.code == r12.code);

    opd->buf[1] = (((int)scale) << 6) | (RegLoBits(index) << 3) |
                    RegLoBits(base);
    opd->rex |= RegHiBit(index) << 1 | RegHiBit(base);
    opd->len = 2;
}

static void OpdSetDisp8(Opd *opd, int disp) {
    assert(IsIntN(disp, 8));
    assert(opd->len == 1 || opd->len == 2);

    int8_t *p = (int8_t *)&opd->buf[opd->len];
    *p = disp;
    opd->len += sizeof(int8_t);
}

static void OpdSetDisp32(Opd *opd, int disp) {
    assert(opd->len == 1 || opd->len == 2);

    int32_t *p = (int32_t *)(&opd->buf[opd->len]);
    *p = disp;
    opd->len += sizeof(int32_t);
}

// [base + disp/r]
OpdRef Operand0(Opd *opd, Reg base, int32_t disp) {
    opd->rex = 0;
    opd->len = 1;
    if (base.code == rsp.code || base.code == r12.code) {
        // From v8:
        // SIB byte is needed to encode (rsp + offset) or (r12 + offset)
        OpdSetSIB(opd, times_1, rsp, base);
    }

    if (disp == 0 && base.code != rbp.code && base.code != r13.code) {
        OpdSetModRM(opd, 0, base);
    } else if (IsIntN(disp, 8)) {
        OpdSetModRM(opd, 1, base);
        OpdSetDisp8(opd, disp);
    } else {
        OpdSetModRM(opd, 2, base);
        OpdSetDisp32(opd, disp);
    }
    return opd;
}

// [base + index * scale + disp/r]
OpdRef Operand1(Opd *opd, Reg base, Reg index, ScaleFactor scale,
                    int32_t disp) {
    assert(index.code != rsp.code);

    opd->rex = 0;
    opd->len = 1;
    OpdSetSIB(opd, scale, index, base);
    if (disp == 0 && base.code != rbp.code && base.code != r13.code) {
        OpdSetModRM(opd, 0, rsp);
    } else if (IsIntN(disp, 8)) {
        OpdSetModRM(opd, 1, rsp);
        OpdSetDisp8(opd, disp);
    } else {
        OpdSetModRM(opd, 2, rsp);
        OpdSetDisp32(opd, disp);
    }
    return opd;
}

// [index * scale + disp/r]
OpdRef Operand2(Opd *opd, Reg index, ScaleFactor scale, int32_t disp) {
    assert (index.code != rsp.code);

    opd->rex = 0;
    opd->len = 1;
    OpdSetModRM(opd, 0, rsp);
    OpdSetSIB(opd, scale, index, rbp);
    OpdSetDisp32(opd, disp);
    return opd;
}

void Emit_lea(Asm *state, Reg dst, OpdRef src, int size) {
    EmitRex2(state, r_op, dst, src, size);
    EmitB(state, 0x8D);
    EmitOperand_r_op(state, dst, src);
}

void Emit_rdrand(Asm *state, Reg dst, int size) {
    EmitRex1(state, r, dst, size);
    EmitB(state, 0x0F);
    EmitB(state, 0xC7);
    EmitModRM0(state, 6, dst);
}

void Emit_pushq_r(Asm *state, Reg src) {
    EmitOptionalRex32_r(state, src);
    EmitB(state, 0x50 | RegLoBits(src));
}

void Emit_pushq_op(Asm *state, OpdRef opd) {
    EmitOptionalRex32_op(state, opd);
    EmitB(state, 0xff);
    EmitOperand(state, 6, opd);
}

void Emit_pushq_i32(Asm *state, int32_t val) {
    EmitB(state, 0x68);
    EmitDW(state, val);
}

void Emit_pushfq(Asm *state) {
    EmitB(state, 0x9c);
}

void Emit_popq_r(Asm *state, Reg dst) {
    EmitOptionalRex32_r(state, dst);
    EmitB(state, 0x58 | RegLoBits(dst));
}

void Emit_popq_op(Asm *state, OpdRef dst) {
    EmitOptionalRex32_op(state, dst);
    EmitB(state, 0x8F);
    EmitOperand(state, 0, dst);
}

void Emit_popfq(Asm *state) {
    EmitB(state, 0x9D);
}

void Emit_movp0(Asm *state, Reg dst, void *val) {
    EmitRex1(state, r, dst, sizeof(val));
    EmitB(state, 0xB8 | RegLoBits(dst));
    EmitP0(state, val);
}

void Emit_movq_i64(Asm *state, Reg dst, int64_t val) {
    EmitRex64_r(state, dst);
    EmitB(state, 0xB8 | RegLoBits(dst));
    EmitQW(state, val);
}

void Emit_movq_p(Asm *state, Reg dst, void *val) {
    assert(sizeof(val) == sizeof(int64_t));

    EmitRex64_r(state, dst);
    EmitB(state, 0xB8 | RegLoBits(dst));
    EmitQW(state, ((uintptr_t)val));
}

void Emit_movq_r_r(Asm *state, Reg dst, Reg src, int size) {
    if (RegLoBits(dst) == 4) {
        EmitRex2(state, r_r, src, dst, size);
        EmitB(state, 0x89);
        EmitModRM(state, src, dst);
    } else {
        EmitRex2(state, r_r, dst, src, size);
        EmitB(state, 0x8B);
        EmitModRM(state, dst, src);
    }
}

void Emit_movq_r_op(Asm *state, Reg dst, OpdRef src, int size) {
    EmitRex2(state, r_op, dst, src, size);
    EmitB(state, 0x8B);
    EmitOperand_r_op(state, dst, src);
}

void Emit_movq_op_r(Asm *state, OpdRef dst, Reg src, int size) {
    EmitRex2(state, r_op, src, dst, size);
    EmitB(state, 0x89);
    EmitOperand_r_op(state, src, dst);
}

void Emit_movq_r_i(Asm *state, Reg dst, Imm src, int size) {
    EmitRex1(state, r, dst, size);
    if (size == sizeof(uint64_t)) {
        EmitB(state, 0xC7);
        EmitModRM0(state, 0x0, dst);
    } else {
        assert(size == sizeof(uint32_t));
        EmitB(state, 0xB8 + RegLoBits(dst));
    }
    EmitDW(state, src.value);
}

void Emit_movq_op_i(Asm *state, OpdRef dst, Imm src, int size) {
    EmitRex1(state, op, dst, size);
    EmitB(state, 0xC7);
    EmitOperand(state, 0x0, dst);
    EmitDW(state, src.value);
}

void Emit_movb_r_r(Asm *state, Reg dst, Reg src) {
    if (!RegIsByte(dst)) {
        EmitRex32_r_r(state, src, dst);
    } else {
        EmitOptionalRex32_r_r(state, src, dst);
    }
    EmitB(state, 0x88);
    EmitModRM(state, src, dst);
}

void Emit_movb_r_op(Asm *state, Reg dst, OpdRef src) {
    if (!RegIsByte(dst)) {
        // Register is not one of al, bl, cl, dl. Its encoding needs REX
        EmitRex32_op(state, src);
    } else {
        EmitOptionalRex32_op(state, src);
    }
    EmitB(state, 0x8A);
    EmitOperand_r_op(state, dst, src);
}

void Emit_movb_op_r(Asm *state, OpdRef dst, Reg src) {
    if (!RegIsByte(src)) {
        EmitRex32_r_op(state, src, dst);
    } else {
        EmitOptionalRex32_r_op(state, src, dst);
    }
    EmitB(state, 0x88);
    EmitOperand_r_op(state, src, dst);
}

void Emit_movb_r_i(Asm *state, Reg dst, Imm src) {
    if (!RegIsByte(dst)) {
        EmitRex32_r(state, dst);
    }
    EmitB(state, 0xB0 + RegLoBits(dst));
    EmitB(state, src.value);
}

void Emit_movb_op_i(Asm *state, OpdRef dst, Imm src) {
    EmitOptionalRex32_op(state, dst);
    EmitB(state, 0xC6);
    EmitOperand(state, 0x00, dst);
    EmitB(state, src.value);
}

void Emit_movw_r_r(Asm *state, Reg dst, Reg src) {
    EmitB(state, 0x66);
    EmitOptionalRex32_r_r(state, src, dst);
    EmitB(state, 0x89);
    EmitModRM(state, src, dst);
}

void Emit_movw_r_op(Asm *state, Reg dst, OpdRef src) {
    EmitB(state, 0x66);
    EmitOptionalRex32_r_op(state, dst, src);
    EmitB(state, 0x8B);
    EmitOperand_r_op(state, dst, src);
}

void Emit_movw_op_r(Asm *state, OpdRef dst, Reg src) {
    EmitB(state, 0x66);
    EmitOptionalRex32_r_op(state, src, dst);
    EmitB(state, 0x89);
    EmitOperand_r_op(state, src, dst);
}

void Emit_movw_r_i(Asm *state, Reg dst, Imm src) {
    //EmitRex32_r(state, dst);
    EmitB(state, 0x66);
    EmitRex32_r(state, dst);
    EmitB(state, 0xB8 + RegLoBits(dst));
    EmitW(state, src.value);
}

void Emit_movw_op_i(Asm *state, OpdRef dst, Imm src) {
    EmitB(state, 0x66);
    EmitOptionalRex32_op(state, dst);
    EmitB(state, 0xC7);
    EmitOperand(state, 0x00, dst);
    EmitB(state, (uint8_t)(src.value & 0xff));
    EmitB(state, (uint8_t)(src.value >> 8));
}

void Emit_movzxb_r_r(Asm *state, Reg dst, Reg src) {
    assert(src.code == kRAX || src.code == kRBX || src.code == kRCX ||
           src.code == kRDX);
    EmitOptionalRex32_r_r(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0xB6);
    EmitModRM(state, dst, src);
}

void Emit_movzxb_r_op(Asm *state, Reg dst, OpdRef src) {
    EmitOptionalRex32_r_op(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0xB6);
    EmitOperand_r_op(state, dst, src);
}

void Emit_movzxw_r_r(Asm *state, Reg dst, Reg src) {
    EmitOptionalRex32_r_r(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0xB7);
    EmitModRM(state, dst, src);
}

void Emit_movzxw_r_op(Asm *state, Reg dst, OpdRef src) {
    EmitOptionalRex32_r_op(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0xB7);
    EmitOperand_r_op(state, dst, src);
}

void Emit_movsxb_r_r(Asm *state, Reg dst, Reg src) {
    assert(src.code == kRAX || src.code == kRBX || src.code == kRCX ||
           src.code == kRDX);
    EmitOptionalRex32_r_r(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0xBE);
    EmitModRM(state, dst, src);
}

void Emit_movsxb_r_op(Asm *state, Reg dst, OpdRef src) {
    EmitOptionalRex32_r_op(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0xBE);
    EmitOperand_r_op(state, dst, src);
}

void Emit_movsxw_r_r(Asm *state, Reg dst, Reg src) {
    EmitOptionalRex32_r_r(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0xBF);
    EmitModRM(state, dst, src);
}

void Emit_movsxw_r_op(Asm *state, Reg dst, OpdRef src) {
    EmitOptionalRex32_r_op(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0xBF);
    EmitOperand_r_op(state, dst, src);
}

void Emit_call_l(Asm *state, YILabel *l) {
    EmitB(state, 0xE8);
    if (YILabelIsBound(l)) {
        int offset = YILabelPos(l) - PCOffset(state) - sizeof(uint32_t);
        assert(offset <= 0);
        EmitDW(state, offset);
    } else if (YILabelIsLinked(l)) {
        EmitDW(state, YILabelPos(l));
        YILabelLinkTo(l, PCOffset(state) - sizeof(uint32_t), 1);
    } else {
        assert(YILabelIsUnused(l));
        int32_t curr = PCOffset(state);
        EmitDW(state, curr);
        YILabelLinkTo(l, curr, 1);
    }
}

void Emit_call_p(Asm *state, void *p) {
    // TODO: Record

    // 1110 1000 #32-bit disp
    EmitB(state, 0xE8);
    void *src = state->pc + 4;

    intptr_t disp = p - src;
    assert(IsIntN(disp, 32)); // near call; only to 32bit
    EmitDW(state, disp);
}

void Emit_call_r(Asm *state, Reg addr) {
    // TODO: Record

    // opcode: FF /2 r64
    EmitOptionalRex32_r(state, addr);
    EmitB(state, 0xff);
    EmitModRM0(state, 0x2, addr);
}

void Emit_call_op(Asm *state, OpdRef opd) {
    // TODO: Record

    // opcode: FF /2 m64
    EmitOptionalRex32_op(state, opd);
    EmitB(state, 0xff);
    EmitOperand(state, 0x2, opd);
}

void Emit_ret_i(Asm *state, int val) {
    assert(IsUintN(val, 16));
    if (val == 0) {
        EmitB(state, 0xC3);
    } else {
        EmitB(state, 0xC2);
        EmitB(state, val & 0xFF);
        EmitB(state, (val >> 8) & 0xFF);
    }
}

void Emit_jmp_l(Asm *state, YILabel *l, int is_far) {
    static const int kShortSize = sizeof(int8_t);
    static const int kLongSize = sizeof(int32_t);

    if (YILabelIsBound(l)) {
        int off = YILabelPos(l) - PCOffset(state) - 1;
        assert(off <= 0);

        if (IsIntN(off - kShortSize, 8)) {
            // 1110 1011 #8-bit disp
            EmitB(state, 0xEB);
            EmitB(state, (off - kShortSize) & 0xFF);
        } else {
            // 1110 1001 #32-bit disp
            EmitB(state, 0xE9);
            EmitDW(state, off - kLongSize);
        }
    } else if (!is_far) { // near
        EmitB(state, 0xEB);
        uint8_t disp = 0x0;

        if (YILabelIsNearLinked(l)) {
            int off = YILabelNearLinkPos(l) - PCOffset(state);
            assert(IsIntN(off, 8));
            disp = (uint8_t)(off & 0xFF);
        }
        YILabelLinkTo(l, PCOffset(state), 0);
        EmitB(state, disp);
    } else if (YILabelIsLinked(l)) {
        // 1110 1001 #32-bit disp
        EmitB(state, 0xE9);
        EmitDW(state, YILabelPos(l));
        YILabelLinkTo(l, PCOffset(state) - kLongSize, 1);
    } else {
        assert(YILabelIsUnused(l));
        EmitB(state, 0xE9);

        int32_t curr = PCOffset(state);
        EmitDW(state, curr);
        YILabelLinkTo(l, curr, 1);
    }
}

void Emit_jcc_l(Asm *state, Cond cc, YILabel *l, int is_far) {
    if (cc == Always) {
        Emit_jmp_l(state, l, is_far);
        return;
    }

    if (cc == Never)
        return;

    assert(IsUintN(cc, 4));
    if (YILabelIsBound(l)) {
        static const int kShortSize = 2;
        static const int kLongSize  = 6;

        int off = YILabelPos(l) - PCOffset(state);
        assert(off <= 0);

        if (IsIntN(off - kShortSize, 8)) {
            // 0111 tttn #8-bit disp
            EmitB(state, 0x70 | cc);
            EmitB(state, (off - kShortSize) & 0xFF);
        } else {
            // 0000 1111 1000 tttn #32-bit disp
            EmitB(state, 0x0F);
            EmitB(state, 0x80 | cc);
            EmitDW(state, (off - kLongSize));
        }
    } else if (!is_far) { // near
        // 0111 tttn #8-bit disp
        EmitB(state, 0x70 | cc);
        uint8_t disp = 0x0;

        if (YILabelIsNearLinked(l)) {
            int off = YILabelNearLinkPos(l) - PCOffset(state);
            assert(IsIntN(off, 8));
            disp = (uint8_t)(off & 0xFF);
        }

        YILabelLinkTo(l, PCOffset(state), 0);
        EmitB(state, disp);
    } else if (YILabelIsLinked(l)) {
        // 0000 1111 1000 tttn #32-bit disp
        EmitB(state, 0x0F);
        EmitB(state, 0x80 | cc);
        EmitDW(state, YILabelPos(l));
        YILabelLinkTo(l, PCOffset(state) - sizeof(uint32_t), 1);
    } else {
        assert(YILabelIsUnused(l));
        EmitB(state, 0x0F);
        EmitB(state, 0x80 | cc);

        int32_t curr = PCOffset(state);
        EmitDW(state, curr);
        YILabelLinkTo(l, curr, 1);
    }
}

void Emit_test_r_r(Asm *state, Reg dst, Reg src, int size) {
    if (RegLoBits(src) == 4) {
        EmitRex2(state, r_r, src, dst, size);
        EmitB(state, 0x85);
        EmitModRM(state, src, dst);
    } else {
        EmitRex2(state, r_r, dst, src, size);
        EmitB(state, 0x85);
        EmitModRM(state, dst, src);
    }
}

void Emit_test_r_i(Asm *state, Reg reg, Imm mask, int size) {
    if (IsUintN(mask.value, 8)) {
        // testb(reg, mask);
        return;
    }

    if (reg.code == rax.code) {
        EmitRex1(state, r, rax, size);
        EmitB(state, 0xA9);
        EmitDW(state, mask.value);
    } else {
        EmitRex1(state, r, reg, size);
        EmitB(state, 0xF7);
        EmitModRM0(state, 0x0, reg);
        EmitDW(state, mask.value);
    }
}

void Emit_test_op_r(Asm *state, OpdRef op, Reg reg, int size) {
    EmitRex2(state, r_op, reg, op, size);
    EmitB(state, 0x85);
    EmitOperand_r_op(state, reg, op);
}

void Emit_test_op_i(Asm *state, OpdRef op, Imm mask, int size) {
    if (IsUintN(mask.value, 8)) {
        // testb(op, mask);
        return;
    }
    EmitRex2(state, r_op, rax, op, size);
    EmitB(state, 0xF7);
    EmitOperand_r_op(state, rax, op);
    EmitDW(state, mask.value);
}

void Emit_not_r(Asm *state, Reg dst, int size) {
    EmitRex1(state, r, dst, size);
    EmitB(state, 0xF7);
    EmitModRM0(state, 0x2, dst);
}

void Emit_not_op(Asm *state, OpdRef dst, int size) {
    EmitRex1(state, op, dst, size);
    EmitB(state, 0xF7);
    EmitOperand(state, 0x2, dst);
}

void Emit_neg_r(Asm *state, Reg dst, int size) {
    EmitRex1(state, r, dst, size);
    EmitB(state, 0xF7);
    EmitModRM0(state, 0x3, dst);
}

void Emit_neg_op(Asm *state, OpdRef dst, int size) {
    EmitRex1(state, op, dst, size);
    EmitB(state, 0xF7);
    EmitOperand(state, 0x3, dst);
}

void Emit_shift_r_i(Asm *state, Reg dst, Imm amount, int subcode,
                    int size) {
    assert(size == sizeof(uint64_t) ? IsUintN(amount.value, 6)
           : IsUintN(amount.value, 5));
    if (amount.value == 1) {
        EmitRex1(state, r, dst, size);
        EmitB(state, 0xD1);
        EmitModRM0(state, subcode, dst);
    } else {
        EmitRex1(state, r, dst, size);
        EmitB(state, 0xC1);
        EmitModRM0(state, subcode, dst);
        EmitB(state, amount.value);
    }
}

void Emit_shift_r(Asm *state, Reg dst, int subcode, int size) {
    EmitRex1(state, r, dst, size);
    EmitB(state, 0xD3);
    EmitModRM0(state, subcode, dst);
}

void Emit_movaps_x_x(Asm *state, Xmm dst, Xmm src) {
    if (XmmLoBits(src) == 4) {
        EmitOptionalRex32_x_x(state, dst, src);
        EmitB(state, 0x0F);
        EmitB(state, 0x29);
        EmitOperand_x_x(state, src, dst);
    } else {
        EmitOptionalRex32_x_x(state, dst, src);
        EmitB(state, 0x0F);
        EmitB(state, 0x28);
        EmitOperand_x_x(state, dst, src);
    }
}

void Emit_movaps_x_op(Asm *state, Xmm dst, OpdRef src) {
    EmitOptionalRex32_x_op(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0x28);
    EmitOperand_x_op(state, dst, src);
}

void Emit_movaps_op_x(Asm *state, OpdRef dst, Xmm src) {
    EmitOptionalRex32_x_op(state, src, dst);
    EmitB(state, 0x0F);
    EmitB(state, 0x29);
    EmitOperand_x_op(state, src, dst);
}

void Emit_movss_x_op(Asm *state, Xmm dst, OpdRef src) {
    EmitB(state, 0xF3);
    EmitOptionalRex32_x_op(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0x10);
    EmitOperand_x_op(state, dst, src);
}

void Emit_movss_op_x(Asm *state, OpdRef dst, Xmm src) {
    EmitB(state, 0xF3);
    EmitOptionalRex32_x_op(state, src, dst);
    EmitB(state, 0x0F);
    EmitB(state, 0x11);
    EmitOperand_x_op(state, src, dst);
}

void Emit_movapd_x_x(Asm *state, Xmm dst, Xmm src) {
    EmitB(state, 0x66);
    if (XmmLoBits(src) == 4) {
        EmitOptionalRex32_x_x(state, dst, src);
        EmitB(state, 0x0F);
        EmitB(state, 0x29);
        EmitOperand_x_x(state, src, dst);
    } else {
        EmitOptionalRex32_x_x(state, dst, src);
        EmitB(state, 0x0F);
        EmitB(state, 0x28);
        EmitOperand_x_x(state, dst, src);
    }
}

void Emit_movapd_x_op(Asm *state, Xmm dst, OpdRef src) {
    EmitB(state, 0x66);
    EmitOptionalRex32_x_op(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0x28);
    EmitOperand_x_op(state, dst, src);
}

void Emit_movapd_op_x(Asm *state, OpdRef dst, Xmm src) {
    EmitB(state, 0x66);
    EmitOptionalRex32_x_op(state, src, dst);
    EmitB(state, 0x0F);
    EmitB(state, 0x29);
    EmitOperand_x_op(state, src, dst);
}

void Emit_movsd_x_op(Asm *state, Xmm dst, OpdRef src) {
    EmitB(state, 0xF2);
    EmitOptionalRex32_x_op(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, 0x10);
    EmitOperand_x_op(state, dst, src);
}

void Emit_movsd_op_x(Asm *state, OpdRef dst, Xmm src) {
    EmitB(state, 0xF2);
    EmitOptionalRex32_x_op(state, src, dst);
    EmitB(state, 0x0F);
    EmitB(state, 0x11);
    EmitOperand_x_op(state, src, dst);
}

void EmitOperand(Asm *state, int code, OpdRef addr) {
    assert(IsUintN(code, 3));

    const unsigned len = addr->len;
    assert(len > 0);

    assert((addr->buf[0] & 0x38) == 0);
    state->pc[0] = addr->buf[0] | code << 3;

    for (unsigned i = 1; i < len; i++)
        state->pc[i] = addr->buf[i];
    state->pc += len;
}

void EmitSSEArith_x_x(Asm *state, uint8_t prefix, uint8_t subcode, Xmm dst, Xmm src) {
    if (prefix)
        EmitB(state, prefix);
    EmitOptionalRex32_x_x(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, subcode);
    EmitOperand_x_x(state, dst, src);
}

void EmitSSEArith_x_op(Asm *state, uint8_t prefix, uint8_t subcode, Xmm dst, OpdRef src) {
    if (prefix)
        EmitB(state, prefix);
    EmitOptionalRex32_x_op(state, dst, src);
    EmitB(state, 0x0F);
    EmitB(state, subcode);
    EmitOperand_x_op(state, dst, src);
}


void EmitSSEArith_r_x(Asm *state, uint8_t prefix, uint8_t subcode, Reg dst,
                      Xmm src, int size) {
    if (prefix) {
        EmitB(state, prefix);
    }
    EmitRex2(state, r_x, dst, src, size);
    EmitB(state, 0x0F);
    EmitB(state, subcode);
    EmitOperand_r_x(state, dst, src);
}

void EmitSSEArith_r_op(Asm *state, uint8_t prefix, uint8_t subcode, Reg dst,
                       OpdRef src, int size) {
    if (prefix) {
        EmitB(state, prefix);
    }
    EmitRex2(state, r_op, dst, src, size);
    EmitB(state, 0x0F);
    EmitB(state, subcode);
    EmitOperand_r_op(state, dst, src);
}


void EmitArithOp_r_op(Asm *state, uint8_t op, Reg reg, OpdRef opd, int size) {
    EmitRex2(state, r_op, reg, opd, size);
    EmitB(state, op);
    EmitOperand_r_op(state, reg, opd);
}

void EmitArithOp_r_r(Asm *state, uint8_t op, Reg reg, Reg rm_reg, int size) {
    assert((op & 0xC6) == 2);

    if (RegLoBits(rm_reg) == 4) { // Forces SIB byte.
        // Swap reg and rm_reg and change opcode operand order.
        EmitRex2(state, r_r, rm_reg, reg, size);
        EmitB(state, op ^ 0x20);
        EmitModRM(state, rm_reg, reg);
    } else {
        EmitRex2(state, r_r, reg, rm_reg, size);
        EmitB(state, op);
        EmitModRM(state, reg, rm_reg);
    }
}

void EmitArithOp_r_i(Asm *state, uint8_t subcode, Reg dst, Imm src, int size) {
    EmitRex1(state, r, dst, size);
    if (IsIntN(src.value, 8)) {
        EmitB(state, 0x83);
        EmitModRM0(state, subcode, dst);
        EmitB(state, src.value);
    } else if (dst.code == rax.code) {
        EmitB(state, 0x05 | (subcode << 3));
        EmitDW(state, src.value);
    } else {
        EmitB(state, 0x81);
        EmitModRM0(state, subcode, dst);
        EmitDW(state, src.value);
    }
}

void EmitArithOp_op_i(Asm *state, uint8_t subcode, OpdRef dst, Imm src,
                      int size) {
    EmitRex1(state, op, dst, size);
    if (IsIntN(src.value, 8)) {
        EmitB(state, 0x83);
        EmitOperand(state, subcode, dst);
        EmitB(state, src.value);
    } else {
        EmitB(state, 0x81);
        EmitOperand(state, subcode, dst);
        EmitDW(state, src.value);
    }
}

void EmitArithOp8_r_op(Asm *state, uint8_t op, Reg reg, OpdRef opd) {
    if (!RegIsByte(reg)) {
        EmitRex32_r(state, reg);
    }
    EmitB(state, op);
    EmitOperand_r_op(state, reg, opd);
}

void EmitArithOp8_r_r(Asm *state, uint8_t op, Reg reg, Reg rm_reg) {
    assert((op & 0xC6) == 2);

    if (RegLoBits(rm_reg) == 4) { // forces SIB byte.
        // Swap reg and rm_reg and change opcode operand order
        if (!RegIsByte(rm_reg) || !RegIsByte(reg)) {
            // Register is not on of al, bl, cl, dl. Its encoding needs REX.
            EmitRex32_r_r(state, rm_reg, reg);
        }
        EmitB(state, op ^ 0x02);
        EmitModRM(state, rm_reg, reg);
    } else {
        if (!RegIsByte(reg) || !RegIsByte(rm_reg)) {
            // Register is not one of al, bl, cl, dl. Its encoding needs REX.
            EmitRex32_r_r(state, reg, rm_reg);
        }
        EmitB(state, op);
        EmitModRM(state, reg, rm_reg);
    }
}

void EmitArithOp8_r_i(Asm *state, uint8_t subcode, Reg dst, Imm src) {
    if (!RegIsByte(dst)) {
        EmitRex32_r(state, dst);
    }

    assert(IsIntN(src.value, 8) || IsUintN(src.value, 8));
    EmitB(state, 0x80);
    EmitModRM0(state, subcode, dst);
    EmitB(state, src.value);
}

void EmitArithOp8_op_i(Asm *state, uint8_t subcode, OpdRef dst, Imm src) {
    EmitOptionalRex32_op(state, dst);

    assert(IsIntN(src.value, 8) || IsUintN(src.value, 8));
    EmitB(state, 0x80);
    EmitOperand(state, subcode, dst);
    EmitB(state, src.value);
}

void EmitArithOp16_r_op(Asm *state, uint8_t op, Reg reg, OpdRef opd) {
    EmitB(state, 0x66);
    EmitOptionalRex32_r_op(state, reg, opd);
    EmitB(state, op);
    EmitOperand_r_op(state, reg, opd);
}

void EmitArithOp16_r_r(Asm *state, uint8_t op, Reg reg, Reg rm_reg) {
    assert((op & 0xC6) == 2);

    if (RegLoBits(rm_reg) == 4) { // Forces SIB byte.
        // Swap reg and rm_reg and change opcode operand order.
        EmitB(state, 0x66);
        EmitOptionalRex32_r_r(state, rm_reg, reg);
        EmitB(state, op ^ 0x02);
        EmitModRM(state, rm_reg, reg);
    } else {
        EmitB(state, 0x66);
        EmitOptionalRex32_r_r(state, reg, rm_reg);
        EmitB(state, op);
        EmitModRM(state, reg, rm_reg);
    }
}

void EmitArithOp16_r_i(Asm *state, uint8_t subcode, Reg dst, Imm src) {
    EmitB(state, 0x66);
    EmitOptionalRex32_r(state, dst);

    if (IsIntN(src.value, 8)) {
        EmitB(state, 0x83);
        EmitModRM0(state, subcode, dst);
        EmitB(state, src.value);
    } else if (dst.code == rax.code) {
        EmitB(state, 0x05 | (subcode << 3));
        EmitW(state, src.value);
    } else {
        EmitB(state, 0x81);
        EmitModRM0(state, subcode, dst);
        EmitW(state, src.value);
    }
}

void EmitArithOp16_op_i(Asm *state, uint8_t subcode, OpdRef dst, Imm src) {
    EmitB(state, 0x66); // Operand size override prefix.
    EmitOptionalRex32_op(state, dst);
    if (IsIntN(src.value, 8)) {
        EmitB(state, 0x83);
        EmitOperand(state, subcode, dst);
        EmitB(state, src.value);
    } else {
        EmitB(state, 0x81);
        EmitOperand(state, subcode, dst);
        EmitW(state, src.value);
    }
}

#define AddrAt(state, pos)       ((state)->code + (pos))
#define LongAt(state, pos)       (*((uint32_t*)AddrAt(state, pos)))
#define LongAtPut(state, pos, x) LongAt(state, pos) = (x)

void BindTo(Asm *state, YILabel *l, int pos) {
    assert(!YILabelIsBound(l)); // Label may only be bound once.
    assert(0 <= pos && pos <= PCOffset(state));

    if (YILabelIsLinked(l)) {
        int curr = YILabelPos(l);
        int next = LongAt(state, curr);

        while (next != curr) {
            int i32 = pos - (curr + sizeof(uint32_t));
            LongAtPut(state, curr, i32);
            curr = next;
            next = LongAt(state, next);
        }

        int last_i32 = pos - (curr + sizeof(uint32_t));
        LongAtPut(state, curr, last_i32);
    }

    while (YILabelIsNearLinked(l)) {
        int fixup_pos = YILabelNearLinkPos(l);
        int off_to_next = *AddrAt(state, fixup_pos);
        assert(off_to_next <= 0);

        int disp = pos - (fixup_pos + sizeof(int8_t));
        assert(IsIntN(disp, 8));

        state->code[fixup_pos] = disp;
        if (off_to_next < 0) {
            YILabelLinkTo(l, fixup_pos + off_to_next, 0);
        } else {
            l->near_link_pos = 0;
        }
    }

    YILabelBindTo(l, pos);
}
