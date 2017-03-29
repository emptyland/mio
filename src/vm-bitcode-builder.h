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
    MemorySegment *code() const { return code_; }

    ////////////////////////////////////////////////////////////////////////////
    // debug
    ////////////////////////////////////////////////////////////////////////////
    int debug() { return EmitInstOnly(BC_debug); }

    ////////////////////////////////////////////////////////////////////////////
    // load
    ////////////////////////////////////////////////////////////////////////////
    int load_1b(uint16_t dest, uint16_t segment, int32_t offset) {
        return Emit3Addr(BC_load_1b, dest, segment, offset);
    }

    int load_2b(uint16_t dest, uint16_t segment, int32_t offset) {
        return Emit3Addr(BC_load_2b, dest, segment, offset);
    }

    int load_4b(uint16_t dest, uint16_t segment, int32_t offset) {
        return Emit3Addr(BC_load_4b, dest, segment, offset);
    }

    int load_8b(uint16_t dest, uint16_t segment, int32_t offset) {
        return Emit3Addr(BC_load_8b, dest, segment, offset);
    }

    int load_i32_imm(uint16_t dest, int32_t imm) {
        return Emit3Addr(BC_load_i32_imm, dest, 0, imm);
    }

    int load(CodeLabel *label);

    int load_o(uint16_t dest, int32_t offset) {
        return Emit3Addr(BC_load_o, dest, BC_GLOBAL_OBJECT_SEGMENT, offset);
    }

    ////////////////////////////////////////////////////////////////////////////
    // mov
    ////////////////////////////////////////////////////////////////////////////
    int mov_1b(int16_t dest, int16_t src) {
        return EmitS2Addr(BC_mov_1b, dest, src);
    }

    int mov_2b(int16_t dest, int16_t src) {
        return EmitS2Addr(BC_mov_2b, dest, src);
    }

    int mov_4b(int16_t dest, int16_t src) {
        return EmitS2Addr(BC_mov_4b, dest, src);
    }

    int mov_8b(int16_t dest, int16_t src) {
        return EmitS2Addr(BC_mov_8b, dest, src);
    }

    int mov_o(int16_t dest, int16_t src) {
        return EmitS2Addr(BC_mov_o, dest, src);
    }

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

    int call_val(uint16_t base1, uint16_t base2, int32_t obj) {
        return Emit3Addr(BC_call_val, base1, base2, obj);
    }

    int ret() {
        return EmitInstOnly(BC_ret);
    }

    int frame(int16_t size1, int16_t size2) {
        return EmitS2Addr(BC_frame, size1, size2);
    }

    int create(BCConstructorId id, uint16_t dest, uint16_t op) {
        return Emit3Addr(BC_create, dest, op, id);
    }

    int jmp(CodeLabel *label);

    int jnz(uint16_t cond, CodeLabel *label);

    int jz(uint16_t cond, CodeLabel *label);

    int EmitInstOnly(BCInstruction inst) {
        return EmitBitCode(static_cast<uint64_t>(inst) << 56);
    }

    int Emit2Addr(BCInstruction inst, uint16_t op1, uint16_t op2) {
        return Emit3Addr(inst, op1, op2, 0);
    }

    int EmitS2Addr(BCInstruction inst, int16_t val1, int16_t val2) {
        return EmitBitCode(MakeS2AddrBC(inst, val1, val2));
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

    static uint64_t MakeS2AddrBC(BCInstruction inst, int16_t val1, int16_t val2) {
        return (static_cast<uint64_t>(inst) << 56) |
               ((static_cast<uint64_t>(val1) << 16) & 0xffff0000) |
               (static_cast<uint64_t>(val2) & 0xffff) ;
    }
private:

    MemorySegment *code_;
    int pc_ = 0;
}; // class BitCodeBuilder

} // namespace mio

#endif // MIO_VM_BITCODE_BUILDER_H_
