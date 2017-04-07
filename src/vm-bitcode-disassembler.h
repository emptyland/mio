#ifndef MIO_VM_BITCODE_DISASSEMBLER_H_
#define MIO_VM_BITCODE_DISASSEMBLER_H_


#include "vm-objects.h"
#include "base.h"
#include "glog/logging.h"

namespace mio {

class MemorySegment;
class TextOutputStream;

class BitCodeDisassembler {
public:
    BitCodeDisassembler(TextOutputStream *stream)
        : stream_(DCHECK_NOTNULL(stream)) {}

    void Run(Handle<MIONormalFunction> func);
    void Run(const void *bc, int size);
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
    static int16_t GetVal1(uint64_t bc) {
        return static_cast<int16_t>((bc & 0xffffffff) >> 16);
    }
    static int16_t GetVal2(uint64_t bc) {
        return static_cast<int16_t>(bc & 0xffffffff);
    }

    static void Disassemble(MemorySegment *code, int number_of_inst,
                            TextOutputStream *stream);

    static void Disassemble(MemorySegment *code, int number_of_inst,
                            std::string *buf);

    DISALLOW_IMPLICIT_CONSTRUCTORS(BitCodeDisassembler)
private:
    TextOutputStream *stream_;
};

} // namespace mio

#endif // MIO_VM_BITCODE_DISASSEMBLER_H_
