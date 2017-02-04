#include "object.h"
#include "base.h"

namespace mio {

void Object::ToString(std::string *s) {
#define DEF_TO_STRING(name, clazz)              \
    case VALUE_##name:                          \
        static_cast<clazz*>(this)->ToString(s); \
        break;

    switch(kind()) {
    DECL_VAL_KIND(DEF_TO_STRING)

    default:
        NOREACHED() << "unknown kind: " << kind();
        break;
    }

#undef DEF_TO_STRING
}

void Number::ToString(std::string *s) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%f", native());
    s->append(buf);
}

void Id::ToString(std::string *s) {
    s->append(c_str());
}

// (a b c d e f)
// car: a
// cdr: b c d...
// car: d
// cdr: c d e...
// car: c
void Pair::ToString(std::string *s) {
    s->append("(");
    auto more = this;
    while (!more->empty()) {
        more->car()->ToString(s);
        s->append(" ");
        more = more->cdr();
    }
    s->append(")");
}

void Boolean::ToString(std::string *s) {
    if (is_true()) {
        s->append("#t");
    } else if (is_false()) {
        s->append("#f");
    } else if (is_undef()) {
        s->append("#u");
    } else {
        NOREACHED();
    }
}

} // namespace mio
