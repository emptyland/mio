#include "vm-bitcode-disassembler.h"
#include "vm-memory-segment.h"
#include "vm-bitcode.h"
#include "text-output-stream.h"
#include "memory-output-stream.h"

namespace mio {

void BitCodeDisassembler::Run(int pc, int len) {
    DCHECK_GE(pc + len, 0);
    DCHECK_LE(pc + len, n_inst_);

    for (int i = pc; i < len; ++i) {
        if (info_) {
            auto iter = info_->find(i);
            if (iter != info_->end()) {
                stream_->Printf("---- %s ----\n", std::get<0>(iter->second).c_str());
            }
        }

        stream_->Printf("[%03d] ", i);
        Disassemble(*static_cast<uint64_t *>(code_->offset(i * sizeof(uint64_t))));
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
            stream_->Printf("[%u] %d@(%u)", GetOp1(bc), GetImm32(bc), GetOp2(bc));
            break;

        case BC_load_i32_imm:
            stream_->Printf("[%u] %d", GetOp1(bc), GetImm32(bc));
            break;

        case BC_add_i8:
        case BC_add_i16:
        case BC_add_i32:
        case BC_add_i64:
            stream_->Printf("[%u] [%u] [%u]", GetOp1(bc), GetOp2(bc),
                            GetOp3(bc));
            break;

        case BC_add_i8_imm:
        case BC_add_i16_imm:
        case BC_add_i32_imm:
            stream_->Printf("[%u] [%u] %d", GetOp1(bc), GetOp2(bc),
                            GetImm32(bc));
            break;

        case BC_call:
            stream_->Printf("[%u] [%u] @%d", GetOp1(bc), GetOp2(bc),
                            GetImm32(bc));
            break;

        case BC_call_val:
            stream_->Printf("%u %u %d", GetOp1(bc), GetOp2(bc), GetImm32(bc));
            break;

        case BC_frame:
            stream_->Printf("+%d +%d", GetVal1(bc), GetVal2(bc));
            break;

        case BC_ret:
            break;

        default:
            DLOG(FATAL) << "instruction: " << metadata->text << " not support yet";
            break;
    }
}

/*static*/
void BitCodeDisassembler::Disassemble(MemorySegment *code, int number_of_inst,
                                      TextOutputStream *stream) {
    BitCodeDisassembler dis(code, number_of_inst, nullptr, stream);
    dis.Run();
}

/*static*/
void BitCodeDisassembler::Disassemble(MemorySegment *code, int number_of_inst,
                                      std::string *buf) {
    MemoryOutputStream stream(buf);
    Disassemble(code, number_of_inst, &stream);
}

} // namespace mio
