#include "vm-bitcode-disassembler.h"
#include "vm-memory-segment.h"
#include "vm-bitcode.h"
#include "text-output-stream.h"
#include "memory-output-stream.h"

namespace mio {

void BitCodeDisassembler::Run(Handle<MIOGeneratedFunction> fn) {
    stream_->Printf("-----[ %s ]-----\n", fn->GetName() ? fn->GetName()->GetData() : "null");

    Run(fn->GetCode(),
        !fn->GetDebugInfo() ? nullptr : fn->GetDebugInfo()->pc_to_position,
        fn->GetCodeSize());
}

void BitCodeDisassembler::Run(const void *buf, const int *p2p, int size) {
    auto bc = static_cast<const uint64_t *>(buf);
    for (int i = 0; i < size; ++i) {
        if (p2p) {
            stream_->Printf("[%03d]:%d ", i, p2p[i]);
        } else {
            stream_->Printf("[%03d] ", i);
        }
        Disassemble(bc[i]);
        stream_->Write("\n");
    }
}

void BitCodeDisassembler::Disassemble(uint64_t bc) {
    auto cmd = GetInst(bc);
    DCHECK_GE(cmd, 0);
    DCHECK_LT(cmd, MAX_BC_INSTRUCTIONS);

    auto metadata = &kInstructionMetadata[cmd];
    stream_->Printf("%s ", metadata->text);

    switch (static_cast<BCInstruction>(cmd)) {
        case BC_debug:
            break;

        case BC_mov_1b:
        case BC_mov_2b:
        case BC_mov_4b:
        case BC_mov_8b:
        case BC_mov_o:
            stream_->Printf("[%d] [%d]", GetVal1(bc), GetVal2(bc));
            break;

        case BC_load_1b:
        case BC_load_2b:
        case BC_load_4b:
        case BC_load_8b:
        case BC_load_o:
            stream_->Printf("[%u] %d@(%s)", GetOp1(bc), GetImm32(bc), kSegmentText[GetOp2(bc)]);
            break;

        case BC_load_i8_imm:
        case BC_load_i16_imm:
        case BC_load_i32_imm:
            stream_->Printf("[%u] %d", GetOp1(bc), GetImm32(bc));
            break;

        case BC_store_1b:
        case BC_store_2b:
        case BC_store_4b:
        case BC_store_8b:
        case BC_store_o:
            stream_->Printf("%d@(%s) [%u]", GetImm32(bc), kSegmentText[GetOp2(bc)], GetOp1(bc));
            break;

        case BC_cmp_i8:
        case BC_cmp_i16:
        case BC_cmp_i32:
        case BC_cmp_i64:
        case BC_cmp_f32:
        case BC_cmp_f64:
            DCHECK_LT(GetOp1(bc), MAX_CC_COMPARATORS);
            stream_->Printf("<%s> [%u] [%d] [%d]",
                            kComparatorText[GetOp1(bc)],
                            GetOp2(bc),
                            GetVal1(bc),
                            GetVal2(bc));
            break;

        case BC_or_i8:
        case BC_or_i16:
        case BC_or_i32:
        case BC_or_i64:
            // fallthrough
        case BC_xor_i8:
        case BC_xor_i16:
        case BC_xor_i32:
        case BC_xor_i64:
            // fallthrough
        case BC_and_i8:
        case BC_and_i16:
        case BC_and_i32:
        case BC_and_i64:
            // fallthrough
        case BC_shl_i8:
        case BC_shl_i16:
        case BC_shl_i32:
        case BC_shl_i64:
            // fallthrough
        case BC_shr_i8:
        case BC_shr_i16:
        case BC_shr_i32:
        case BC_shr_i64:
            // fallthrough
        case BC_ushr_i8:
        case BC_ushr_i16:
        case BC_ushr_i32:
        case BC_ushr_i64:
            // fallthrough
        case BC_add_i8:
        case BC_add_i16:
        case BC_add_i32:
        case BC_add_i64:
        case BC_add_f32:
        case BC_add_f64:
            // fallthrough
        case BC_sub_i8:
        case BC_sub_i16:
        case BC_sub_i32:
        case BC_sub_i64:
        case BC_sub_f32:
        case BC_sub_f64:
            // fallthrough
        case BC_mul_i8:
        case BC_mul_i16:
        case BC_mul_i32:
        case BC_mul_i64:
        case BC_mul_f32:
        case BC_mul_f64:
            // fallthrough
        case BC_div_i8:
        case BC_div_i16:
        case BC_div_i32:
        case BC_div_i64:
        case BC_div_f32:
        case BC_div_f64:
            stream_->Printf("[%u] [%u] [%u]", GetOp1(bc), GetOp2(bc),
                            GetOp3(bc));
            break;

        case BC_inv_i8:
        case BC_inv_i16:
        case BC_inv_i32:
        case BC_inv_i64:
            // fallthrough
        case BC_logic_not:
            stream_->Printf("[%u] [%u]", GetOp1(bc), GetOp2(bc));
            break;

        case BC_shl_i8_imm:
        case BC_shl_i16_imm:
        case BC_shl_i32_imm:
        case BC_shl_i64_imm:
            // fallthrough
        case BC_shr_i8_imm:
        case BC_shr_i16_imm:
        case BC_shr_i32_imm:
        case BC_shr_i64_imm:
            // fallthrough
        case BC_ushr_i8_imm:
        case BC_ushr_i16_imm:
        case BC_ushr_i32_imm:
        case BC_ushr_i64_imm:
            // fallthrough
        case BC_add_i8_imm:
        case BC_add_i16_imm:
        case BC_add_i32_imm:
            stream_->Printf("[%u] [%u] %d", GetOp1(bc), GetOp2(bc),
                            GetImm32(bc));
            break;

        case BC_sext_i8:
        case BC_sext_i16:
        case BC_sext_i32:
            // fallthrough
        case BC_trunc_i16:
        case BC_trunc_i32:
        case BC_trunc_i64:
            // fallthrough
        case BC_fpext_f32:
        case BC_fpext_f64:
            // fallthrough
        case BC_fptrunc_f32:
        case BC_fptrunc_f64:
            // fallthrough
        case BC_sitofp_i8:
        case BC_sitofp_i16:
        case BC_sitofp_i32:
        case BC_sitofp_i64:
            // fallthrough
        case BC_fptosi_f32:
        case BC_fptosi_f64:
            stream_->Printf("[%u] <%u> [%u]", GetOp1(bc), GetOp2(bc) * 8, GetImm32(bc));
            break;

        case BC_close_fn:
            stream_->Printf("[%u]", GetOp1(bc));
            break;

        case BC_call:
            stream_->Printf("[%u] [%u] @%d", GetOp1(bc), GetOp2(bc),
                            GetImm32(bc));
            break;

        case BC_call_val:
            stream_->Printf("%u %u [%d]", GetOp1(bc), GetOp2(bc), GetImm32(bc));
            break;

        case BC_frame:
            stream_->Printf("+%u +%u %d %d", GetOp1(bc), GetOp2(bc),
                            GetVal1(bc), GetVal2(bc));
            break;

        case BC_ret:
            break;

        case BC_oop:
            DCHECK_LT(GetOp1(bc), MAX_OO_OPERATORS);
            stream_->Printf("\'%s\' [%u] %d %d",
                            kObjectOperatorText[GetOp1(bc)],
                            GetOp2(bc),
                            GetVal1(bc),
                            GetVal2(bc));
            break;

        case BC_jz:
        case BC_jnz:
            if (GetOp1(bc) > 0) {
                stream_->Printf("[%u] %d #%d", GetOp2(bc), GetImm32(bc), GetOp1(bc));
            } else {
                stream_->Printf("[%u] %d", GetOp2(bc), GetImm32(bc));
            }
            break;

        case BC_jmp:
            if (GetOp1(bc) > 0 && GetOp2(bc) > 0) {
                stream_->Printf("%d #%d #%d", GetImm32(bc), GetOp1(bc), GetOp2(bc));
            } else {
                stream_->Printf("%d", GetImm32(bc));
            }
            break;

        case BC_loop_entry:
            stream_->Printf("%d@native #%d", GetImm32(bc), GetOp2(bc));
            break;

        default:
            DLOG(FATAL) << "instruction: " << metadata->text << " not support yet";
            break;
    }
}

/*static*/
void BitCodeDisassembler::Disassemble(MemorySegment *code, int number_of_inst,
                                      TextOutputStream *stream) {
    BitCodeDisassembler dis(stream);
    dis.Run(code->offset(0), nullptr, number_of_inst);
}

/*static*/
void BitCodeDisassembler::Disassemble(MemorySegment *code, int number_of_inst,
                                      std::string *buf) {
    MemoryOutputStream stream(buf);
    Disassemble(code, number_of_inst, &stream);
}

/*static*/
void BitCodeDisassembler::Disassemble(Handle<MIOGeneratedFunction> fn,
                                      std::string *buf) {
    MemoryOutputStream stream(buf);
    BitCodeDisassembler dis(&stream);
    dis.Run(fn);
}

} // namespace mio
