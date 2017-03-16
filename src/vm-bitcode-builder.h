#ifndef MIO_VM_BITCODE_BUILDER_H_
#define MIO_VM_BITCODE_BUILDER_H_

#include "vm-memory-segment.h"
#include "vm-bitcode.h"

namespace mio {

class MemorySegment;
class CodeLabel;

class BitCodeBuilder {
public:
    BitCodeBuilder(MemorySegment *code) : code_(DCHECK_NOTNULL(code)) {}

    void BindTo(CodeLabel *label, int position);
    void BindNow(CodeLabel *label) { BindTo(label, pc_); }

    DEF_GETTER(int, pc);

    ////////////////////////////////////////////////////////////////////////////
    // debug
    ////////////////////////////////////////////////////////////////////////////
    int debug() { return EmitInstOnly(BC_debug); }

    ////////////////////////////////////////////////////////////////////////////
    // load
    ////////////////////////////////////////////////////////////////////////////
    int load_i32_imm(int32_t imm) {
        return Emit3Addr(BC_load_i32_imm, 0, 0, imm);
    }

    int load(CodeLabel *label);

    ////////////////////////////////////////////////////////////////////////////
    // add
    ////////////////////////////////////////////////////////////////////////////
    int add_i8(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_add_i8, result, lhs, rhs);
    }

    int add_i8_imm(uint16_t result, uint16_t lhs, int8_t imm) {
        return Emit3Addr(BC_add_i8_imm, result, lhs, imm);
    }

    int add_i32_imm(uint16_t result, uint16_t lhs, int32_t imm) {
        return Emit3Addr(BC_add_i32_imm, result, lhs, imm);
    }

    ////////////////////////////////////////////////////////////////////////////
    // call
    ////////////////////////////////////////////////////////////////////////////
    int call(uint16_t base1, uint16_t base2, CodeLabel *label);

    int EmitInstOnly(BCInstruction inst) {
        return EmitBitCode(static_cast<uint64_t>(inst) << 56);
    }

    int Emit2Addr(BCInstruction inst, uint16_t op1, uint16_t op2) {
        return Emit3Addr(inst, op1, op2, 0);
    }

    // op(8bits) result(12bits) op1(12bits) op2(32bits)
    int Emit3Addr(BCInstruction inst, uint16_t result, uint16_t op1,
                  int32_t op2) {
        return EmitBitCode(Make3AddrBC(inst, result, op1, op2));
    }

    int EmitBitCode(uint64_t bc) {
        *static_cast<uint64_t *>(code_->Advance(sizeof(bc))) = bc;
        return pc_++;
    }

    static uint64_t Make3AddrBC(BCInstruction inst, uint16_t result,
                                uint16_t op1, int32_t op2) {
        DCHECK_LE(result, 0xfff);
        DCHECK_LE(op1, 0xfff);
        return (static_cast<uint64_t>(inst) << 56)           |
               (static_cast<uint64_t>(result & 0xfff) << 44) |
               (static_cast<uint64_t>(op1 & 0xfff) << 32)    |
               op2;
    }
private:
    MemorySegment *code_;
    int pc_ = 0;
}; // class BitCodeBuilder

} // namespace mio

#endif // MIO_VM_BITCODE_BUILDER_H_
