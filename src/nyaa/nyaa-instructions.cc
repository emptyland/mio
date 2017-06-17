#include "nyaa-instructions.h"
#include "text-output-stream.h"

namespace mio {

//void NValue::Kill(Zone *zone) {
//    set_flag(kIsDead);
//
//    for (int i = 0; i < operand_size(); ++i) {
//        auto op = operand(i);
//        if (!op) {
//            continue;
//        }
//
//        auto first = op->used_values_;
//        if (first != nullptr && first->value()->test_flag(kIsDead)) {
//            op->used_values_ = first->tail();
//        }
//    }
//}

void NAdd::ToString(TextOutputStream *stream) const {
    lhs()->ToString(stream);
    stream->Write(" ");
}

} // namespace mio
