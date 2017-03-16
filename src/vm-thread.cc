#include "vm-thread.h"
#include "vm-memory-segment.h"
#include "vm-bitcode-disassembler.h"
#include "vm-bitcode.h"
#include "vm.h"
#include "glog/logging.h"

namespace mio {

Thread::Thread(VM *vm)
    : vm_(DCHECK_NOTNULL(vm)) {
}

Thread::~Thread() {

}

void Thread::Execute(int pc, bool *ok) {
    DCHECK_LT(pc * 8, vm_->code_->size());
    pc_ = pc;

    uint64_t *pbc = static_cast<uint64_t *>(vm_->code_->offset(pc_));
    while (!should_exit_) {
        auto bc = *pbc++;

        switch (BitCodeDisassembler::GetInst(bc)) {
            case BC_debug:
                *ok = false;
                exit_code_ = DEBUGGING;
                return;

            case BC_load_i32_imm: {
                auto dest = BitCodeDisassembler::GetOp1(bc);
                auto imm32 = BitCodeDisassembler::GetImm32(bc);

                
            } break;
        }
    }
}

} // namespace mio
