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

    int pc() const { return code_->size() / sizeof(uint64_t); }

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

    int load_i8_imm(uint16_t dest, int8_t imm) {
        return Emit3Addr(BC_load_i8_imm, dest, 0, imm);
    }

    int load_i16_imm(uint16_t dest, int16_t imm) {
        return Emit3Addr(BC_load_i16_imm, dest, 0, imm);
    }

    int load_i32_imm(uint16_t dest, int32_t imm) {
        return Emit3Addr(BC_load_i32_imm, dest, 0, imm);
    }

    int load_o(uint16_t dest, uint16_t segment, int32_t offset) {
        return Emit3Addr(BC_load_o, dest, segment, offset);
    }

    int store_1b(int32_t dest, uint16_t segment, uint16_t src) {
        return Emit3Addr(BC_store_1b, src, segment, dest);
    }

    int store_2b(int32_t dest, uint16_t segment, uint16_t src) {
        return Emit3Addr(BC_store_2b, src, segment, dest);
    }

    int store_4b(int32_t dest, uint16_t segment, uint16_t src) {
        return Emit3Addr(BC_store_4b, src, segment, dest);
    }

    int store_8b(int32_t dest, uint16_t segment, uint16_t src) {
        return Emit3Addr(BC_store_8b, src, segment, dest);
    }

    int store_o(int32_t dest, uint16_t segment, uint16_t src) {
        return Emit3Addr(BC_store_o, src, segment, dest);
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
    // arithmetic
    ////////////////////////////////////////////////////////////////////////////
    int cmp_i8(BCComparator op, uint16_t result, int16_t lhs, int16_t rhs) {
        return Emit4Op(BC_cmp_i8, op, result, lhs, rhs);
    }

    int cmp_i16(BCComparator op, uint16_t result, int16_t lhs, int16_t rhs) {
        return Emit4Op(BC_cmp_i16, op, result, lhs, rhs);
    }

    int cmp_i32(BCComparator op, uint16_t result, int16_t lhs, int16_t rhs) {
        return Emit4Op(BC_cmp_i32, op, result, lhs, rhs);
    }

    int cmp_i64(BCComparator op, uint16_t result, int16_t lhs, int16_t rhs) {
        return Emit4Op(BC_cmp_i64, op, result, lhs, rhs);
    }

    int cmp_f32(BCComparator op, uint16_t result, int16_t lhs, int16_t rhs) {
        return Emit4Op(BC_cmp_f32, op, result, lhs, rhs);
    }

    int cmp_f64(BCComparator op, uint16_t result, int16_t lhs, int16_t rhs) {
        return Emit4Op(BC_cmp_f64, op, result, lhs, rhs);
    }

    int add_i8(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_add_i8, result, lhs, rhs);
    }

    int add_i16(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_add_i16, result, lhs, rhs);
    }

    int add_i32(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_add_i32, result, lhs, rhs);
    }

    int add_i64(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_add_i64, result, lhs, rhs);
    }

    int add_f32(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_add_f32, result, lhs, rhs);
    }

    int add_f64(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_add_f64, result, lhs, rhs);
    }

    int add_i8_imm(uint16_t result, uint16_t lhs, int8_t imm) {
        return Emit3Addr(BC_add_i8_imm, result, lhs, imm);
    }

    int add_i16_imm(uint16_t result, uint16_t lhs, int16_t imm) {
        return Emit3Addr(BC_add_i16_imm, result, lhs, imm);
    }

    int add_i32_imm(uint16_t result, uint16_t lhs, int32_t imm) {
        return Emit3Addr(BC_add_i32_imm, result, lhs, imm);
    }

    int sub_i8(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_sub_i8, result, lhs, rhs);
    }

    int sub_i16(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_sub_i16, result, lhs, rhs);
    }

    int sub_i32(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_sub_i32, result, lhs, rhs);
    }

    int sub_i64(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_sub_i64, result, lhs, rhs);
    }

    int sub_f32(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_sub_f64, result, lhs, rhs);
    }

    int sub_f64(uint16_t result, uint16_t lhs, uint16_t rhs) {
        return Emit3Addr(BC_sub_f64, result, lhs, rhs);
    }

    ////////////////////////////////////////////////////////////////////////////
    // type cast
    ////////////////////////////////////////////////////////////////////////////
    // bytes = output size
    int sext_i32(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_sext_i32, result, bytes, input);
    }

    int sext_i16(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_sext_i16, result, bytes, input);
    }

    int sext_i8(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_sext_i8, result, bytes, input);
    }

    int trunc_i16(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_trunc_i16, result, bytes, input);
    }

    int trunc_i32(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_trunc_i32, result, bytes, input);
    }

    int trunc_i64(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_trunc_i64, result, bytes, input);
    }

    int fptrunc_f32(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_fptrunc_f32, result, bytes, input);
    }

    int fptrunc_f64(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_fptrunc_f64, result, bytes, input);
    }

    int fpext_f32(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_fpext_f32, result, bytes, input);
    }

    int fpext_f64(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_fpext_f64, result, bytes, input);
    }

    int fptosi_f32(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_fptosi_f32, result, bytes, input);
    }

    int fptosi_f64(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_fptosi_f64, result, bytes, input);
    }

    int sitofp_i8(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_sitofp_i8, result, bytes, input);
    }

    int sitofp_i16(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_sitofp_i16, result, bytes, input);
    }

    int sitofp_i32(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_sitofp_i32, result, bytes, input);
    }

    int sitofp_i64(uint16_t result, uint16_t bytes, uint16_t input) {
        return Emit3Addr(BC_sitofp_i64, result, bytes, input);
    }

    ////////////////////////////////////////////////////////////////////////////
    // call
    ////////////////////////////////////////////////////////////////////////////
    int close_fn(uint16_t fn) {
        return Emit3Addr(BC_close_fn, fn, 0, 0);
    }

    int call_val(uint16_t base1, uint16_t base2, int32_t obj) {
        return Emit3Addr(BC_call_val, base1, base2, obj);
    }

    int ret() {
        return EmitInstOnly(BC_ret);
    }

    int frame(uint16_t size1, uint16_t size2, int16_t clean2) {
        return Emit4Op(BC_frame, size1, size2, 0, clean2);
    }

    void frame(int pc, uint16_t size1, uint16_t size2, int16_t clean2) {
        FillPlacement(pc, Make4OpBC(BC_frame, size1, size2, 0, clean2));
    }

    int oop(BCObjectOperatorId id, uint16_t result, int16_t val1, int16_t val2) {
        return Emit4Op(BC_oop, id, result, val1, val2);
    }

    int jmp(int32_t delta) {
        return Emit3Addr(BC_jmp, 0, 0, delta);
    }

    void jmp_fill(int pc, int32_t delta) {
        FillPlacement(pc, Make3AddrBC(BC_jmp, 0, 0, delta));
    }

    int jnz(uint16_t cond, int32_t delta) {
        return Emit3Addr(BC_jnz, 0, cond, delta);
    }

    void jnz_fill(int pc, uint16_t cond, int32_t delta) {
        FillPlacement(pc, Make3AddrBC(BC_jnz, 0, cond, delta));
    }

    int jz(uint16_t cond, int32_t delta) {
        return Emit3Addr(BC_jz, 0, cond, delta);
    }

    void jz_fill(int pc, uint16_t cond, int32_t delta) {
        FillPlacement(pc, Make3AddrBC(BC_jz, 0, cond, delta));
    }

    ////////////////////////////////////////////////////////////////////////////
    // [common]
    ////////////////////////////////////////////////////////////////////////////
    void FillPlacement(int pc, uint64_t bc) {
        code_->Set(pc * sizeof(bc), bc);
    }

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

    int Emit4Op(BCInstruction inst, uint16_t id, uint16_t result,
                int16_t val1, int16_t val2) {
        return EmitBitCode(Make4OpBC(inst, id, result, val1, val2));
    }

    int EmitBitCode(uint64_t bc) {
        auto pos = pc();
        *static_cast<uint64_t *>(code_->Advance(sizeof(bc))) = bc;
        return pos;
    }

    static uint64_t Make3AddrBC(BCInstruction inst, uint16_t result,
                                uint16_t op1, int32_t op2) {
        DCHECK_LE(result, 0xfff);
        DCHECK_LE(op1, 0xfff);
        return (static_cast<uint64_t>(inst) << 56)           |
               (static_cast<uint64_t>(result & 0xfff) << 44) |
               (static_cast<uint64_t>(op1 & 0xfff) << 32)    |
               (static_cast<uint64_t>(op2) & 0xffffffff);
    }

    static uint64_t Make4OpBC(BCInstruction inst, uint16_t id, uint16_t result,
                              int16_t val1, int16_t val2) {
        DCHECK_LE(id, 0xfff);
        DCHECK_LE(result, 0xfff);
        return (static_cast<uint64_t>(inst) << 56)                |
               (static_cast<uint64_t>(id & 0xfff) << 44)          |
               (static_cast<uint64_t>(result & 0xfff) << 32)      |
               ((static_cast<uint64_t>(val1) << 16) & 0xffff0000) |
               (static_cast<uint64_t>(val2) & 0xffff);
    }

    static uint64_t MakeS2AddrBC(BCInstruction inst, int16_t val1, int16_t val2) {
        return (static_cast<uint64_t>(inst) << 56) |
               ((static_cast<uint64_t>(val1) << 16) & 0xffff0000) |
               (static_cast<uint64_t>(val2) & 0xffff) ;
    }
private:
    MemorySegment *code_;

}; // class BitCodeBuilder

} // namespace mio

#endif // MIO_VM_BITCODE_BUILDER_H_
