#ifndef MIO_VM_BITCODE_DISASSEMBLER_H_
#define MIO_VM_BITCODE_DISASSEMBLER_H_

#include "base.h"
#include "glog/logging.h"

namespace mio {

class MemorySegment;
class TextOutputStream;

class BitCodeDisassembler {
public:
    BitCodeDisassembler(MemorySegment *code, int number_of_inst,
                        TextOutputStream *stream)
        : code_(DCHECK_NOTNULL(code))
        , n_inst_(number_of_inst)
        , stream_(DCHECK_NOTNULL(stream)) {}

    void Run() { Run(0, n_inst_); }
    void Run(int pc, int len);
    void Disassemble(uint64_t inst);

    static uint8_t GetInst(uint64_t bc) {
        return static_cast<uint8_t>(bc >> 56);
    }
    static uint16_t GetOp1(uint64_t bc) {
        return static_cast<uint16_t>((bc >> 44) & 0xfff);
    }
    static uint16_t GetOp2(uint64_t bc) {
        return static_cast<uint16_t>((bc >> 32) & 0xfff);
    }
    static uint16_t GetOp3(uint64_t bc) {
        return static_cast<uint16_t>(bc & 0xfff);
    }
    static int32_t GetImm32(uint64_t bc) {
        return static_cast<int32_t>(bc & 0xffffffff);
    }

    static void Disassemble(MemorySegment *code, int number_of_inst,
                            TextOutputStream *stream);

    static void Disassemble(MemorySegment *code, int number_of_inst,
                            std::string *buf);

    DISALLOW_IMPLICIT_CONSTRUCTORS(BitCodeDisassembler)
private:
    MemorySegment *code_;
    int n_inst_;
    TextOutputStream *stream_;
};

} // namespace mio

#endif // MIO_VM_BITCODE_DISASSEMBLER_H_
