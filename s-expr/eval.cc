#include "expr.h"

Entry *Eval(Entry *expr, bool *ok) {
    if (expr == EmptyList()) {
        return Unset();
    }

    if (expr->car.kind != VALUE_ID) {
        Errorf(ok, "");
        return Unset();
    }

    // TODO:

    switch (closure_code) {
    case C_DEF:
        return MakeClosure(expr, ok);

    case C_SET:
        return SetValue(expr, ok);
    }
}

// (import io)
// (import foo)
//
// (io.display 'hello')
Entry *MakeClosure

#define CHECK_OK ok); if (!*ok) { return 0; } (void)0

// (set [id] (expr))
Entry *DelcareValue(Entry *expr, bool *ok) {
    auto ctx = CurrentScope();
    auto id = Cadar(expr, CHECK_OK);
    auto rhs = Cddar(expr, CHECK_OK);

    if (ctx->ExistsName(id->car.u.txt)) {
        *ok =false;
        return nullptr;
    }

    auto val = Eval(rhs, CHECK_OK);
    ctx->AddSymbol(id->car.u.txt, val, CHECK_OK);
    return val;
}
