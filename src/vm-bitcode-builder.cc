#include "vm-bitcode-builder.h"
#include "vm-bitcode-disassembler.h"
#include "code-label.h"

namespace mio {

template<class T>
int BuildJumpLikeBC(CodeLabel *label, int pc, T make) {
    if (label->is_bound()) {

        return make(label->position() - pc);
    } else if (label->is_linked()) {

        auto rv = make(label->position());
        label->LinkTo(pc - 1, true);
        return rv;
    } else {

        DCHECK(label->is_unused());
        auto rv = make(pc);
        label->LinkTo(pc - 1, true);
        return rv;
    }
    return 0;
}

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
    return BuildJumpLikeBC(label, pc_, [&] (int position) {
        return Emit3Addr(BC_load_i32_imm, 0, 0, position);
    });
}

int BitCodeBuilder::call(uint16_t base1, uint16_t base2, CodeLabel *label) {
    return BuildJumpLikeBC(label, pc_, [&] (int position) {
        return Emit3Addr(BC_call, base1, base2, position);
    });
}

int BitCodeBuilder::jmp(CodeLabel *label) {
    return BuildJumpLikeBC(label, pc_, [&] (int position) {
        return Emit3Addr(BC_jmp, 0, 0, position);
    });
}

int BitCodeBuilder::jnz(uint16_t cond, CodeLabel *label) {
    return BuildJumpLikeBC(label, pc_, [&] (int position) {
        return Emit3Addr(BC_jnz, 0, cond, position);
    });
}

int BitCodeBuilder::jz(uint16_t cond, CodeLabel *label) {
    return BuildJumpLikeBC(label, pc_, [&] (int position) {
        return Emit3Addr(BC_jz, 0, cond, position);
    });
}

} // namespace mio
