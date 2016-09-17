#include "expr.h"
#include <string>

namespace mio {

const Entry kEmpty = {
    .val = {
        .kind = VALUE_UNSET,
    },
    .car = nullptr,
    .cdr = nullptr,
}

void ExprToString(Entry *expr, std::string *s) {
    s->append("(");
    for (auto curr = expr; curr != kEmpty; curr = curr->cdr) {
        switch (curr->val.kind) {
        case VALUE_UNSET:
            break;

        case VALUE_ID:
            s->append(curr->val.u.txt->data(),
                      curr->val.u.txt->size());
            break;

        case VALUE_NUMBER: {
            char buf[32];
            snprintf(sizeof(buf), buf, "%f");
            s->append(buf);
        } break;
        }
    }
    s->append(")");
}

} // naemspace mio
