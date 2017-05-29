#include "vm-function-register.h"
#include "vm-code-cache.h"
#include "yui/asm-amd64.h"

namespace mio {

const Reg kPrimitiveStack = r14;
const Reg kObjectStack = r15;

void **FunctionRegister::BuildWarper(MIOString *signature) {
    std::unique_ptr<uint8_t[]> buf(new uint8_t[1024]);
    Asm state;
    state.code = buf.get();
    state.pc   = state.code;
    state.size = 1024;

    Emit_pushq_r(&state, rbp);
    Emit_movq_r_r(&state, rbp, rsp, 8);

    Emit_movq_r_r(&state, rax, RegArgv[1], 8);
    Emit_movq_r_r(&state, kPrimitiveStack, RegArgv[2], 8);
    Emit_movq_r_r(&state, kObjectStack, RegArgv[3], 8);

    int ooff = 0, poff = 0;
    int rarg = 1, xarg = 0;
    Opd op;

    auto sign = signature->Get();
    for (int i = 2; i < sign.n; ++i) {
        if (islower(sign.z[i])) { // object
            Operand0(&op, kObjectStack, ooff);
            Emit_movq_r_op(&state, RegArgv[rarg++], &op, kObjectReferenceSize);
            ooff += kObjectReferenceSize;
        } else if (isdigit(sign.z[i])) {
            switch (sign.z[i]) {
                case '1':
                case '8':
                    Operand0(&op, kPrimitiveStack, poff);
                    Emit_movq_r_op(&state, RegArgv[rarg++], &op, 1);
                    break;
                case '7':
                    Operand0(&op, kPrimitiveStack, poff);
                    Emit_movq_r_op(&state, RegArgv[rarg++], &op, 2);
                    break;
                case '5':
                    Operand0(&op, kPrimitiveStack, poff);
                    Emit_movq_r_op(&state, RegArgv[rarg++], &op, 4);
                    break;
                case '9':
                    Operand0(&op, kPrimitiveStack, poff);
                    Emit_movq_r_op(&state, RegArgv[rarg++], &op, 8);
                    break;
                case '3':
                    Operand0(&op, kPrimitiveStack, poff);
                    Emit_movss_x_op(&state, XmmArgv[xarg++], &op);
                    break;
                case '6':
                    Operand0(&op, kPrimitiveStack, poff);
                    Emit_movsd_x_op(&state, XmmArgv[xarg++], &op);
                    break;
                default:
                    DLOG(FATAL) << "noreached!";
                    break;
            }
            if (sign.z[i] == '6' || sign.z[i] == '9') {
                poff += 8;
            } else {
                poff += 4;
            }
        } else {
            DLOG(FATAL) << "noreached!";
        }
    }

    Operand0(&op, rax, MIONativeFunction::kNativePointerOffset);
    Emit_call_op(&state, &op);

    if (islower(sign.z[0])) { // object
        Operand0(&op, kObjectStack, -kObjectReferenceSize);
        Emit_movq_op_r(&state, &op, rax, kObjectReferenceSize);
    } else if (isdigit(sign.z[0])) { // number
        switch (sign.z[0]) {
            case '1':
            case '8':
                Operand0(&op, kPrimitiveStack, -4);
                Emit_movq_op_r(&state, &op, rax, 1);
                break;
            case '7':
                Operand0(&op, kPrimitiveStack, -4);
                Emit_movq_op_r(&state, &op, rax, 2);
                break;
            case '5':
                Operand0(&op, kPrimitiveStack, -4);
                Emit_movq_op_r(&state, &op, rax, 4);
                break;
            case '9':
                Operand0(&op, kPrimitiveStack, -8);
                Emit_movq_op_r(&state, &op, rax, 8);
                break;
            case '3':
                Operand0(&op, kPrimitiveStack, -4);
                Emit_movss_op_x(&state, &op, xmm0);
                break;
            case '6':
                Operand0(&op, kPrimitiveStack, -8);
                Emit_movsd_op_x(&state, &op, xmm0);
                break;
            default:
                DLOG(FATAL) << "noreached!";
                break;
        }
    } else if (sign.z[0] == '!') {
        // void return
        Emit_xor_r_r(&state, rax, rax, 8);
    } else {
        DLOG(FATAL) << "noreached!";
    }

    Emit_popq_r(&state, rbp);
    Emit_ret_i(&state, 0);

    auto size = static_cast<int>(state.pc - state.code);
    auto code = code_cache_->Allocate(size);
    memcpy(code.data(), state.code, size);
    return code.index();
}

} // namespace mio
