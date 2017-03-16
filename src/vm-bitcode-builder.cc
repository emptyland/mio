#include "vm-bitcode-builder.h"
#include "vm-bitcode-disassembler.h"
#include "code-label.h"

namespace mio {

void BitCodeBuilder::BindTo(CodeLabel *label, int position) {
    DCHECK(!label->is_bound()); // Label may only be bound once.
    DCHECK(0 <= position && position <= pc_);

    if (label->is_linked()) {
        int current = label->position();
        int next = *static_cast<int*>(code_->offset(current * 8));

        while (next != current) {
            int delta = position - current;
            *static_cast<int *>(code_->offset(current * 8)) = delta;
            current = next;
            next = *static_cast<int*>(code_->offset(next * 8));
        }

        int delta = position - current;
        *static_cast<int *>(code_->offset(current * 8)) = delta;
    }

    label->BindTo(position);
}

int BitCodeBuilder::load(CodeLabel *label) {

    if (label->is_bound()) {

        auto off = label->position() - pc_;
        return Emit3Addr(BC_load_i32_imm, 0, 0, off);
    } else if (label->is_linked()) {

        auto rv = Emit3Addr(BC_load_i32_imm, 0, 0, label->position());
        label->LinkTo(pc_ - 1, true);
        return rv;
    } else {

        DCHECK(label->is_unused());
        auto rv = Emit3Addr(BC_load_i32_imm, 0, 0, pc_);
        label->LinkTo(pc_ - 1, true);
        return rv;
    }
    return 0;
}

int BitCodeBuilder::call(uint16_t base1, uint16_t base2, CodeLabel *label) {

    if (label->is_bound()) {

        auto off = label->position() - pc_;
        return Emit3Addr(BC_call, base1, base2, off);
    } else if (label->is_linked()) {

        auto rv = Emit3Addr(BC_call, base1, base2, label->position());
        label->LinkTo(pc_ - 1, true);
        return rv;
    } else {

        DCHECK(label->is_unused());
        auto rv = Emit3Addr(BC_call, base1, base2, pc_);
        label->LinkTo(pc_ - 1, true);
        return rv;
    }
    return 0;
}

} // namespace mio
