#include "object.h"
#include "inline_op_def.h"

#define CHECK_OK ok); if (!*ok) { return 0; } (void)0


Object *Evaluate::Eval(Object *obj, Env *env, bool *ok) {
    switch (obj->kind()) {
    case VALUE_ID:
        return GetOrUndef(static_cast<Id*>(obj), env);

    case VALUE_NUMBER:
    case VALUE_BOOLEAN:
        return obj;

    case VALUE_PAIR:
        return Apply(static_cast<Pair*>(obj), env, ok);

    default:
        return env->Undef();
    }
    NOREACHED();
}

Object *Evaluate::Apply(Pair *expr, Env *env, bool *ok) {
    if (expr->empty()) {
        return env->Undef();
    }

    Object *lambda = nullptr;
    auto maybe_id = Car(expr, env);
    if (maybe_id->kind() == VALUE_ID) {
        auto id = AssertId(ev, CHECK_OK);
        auto op = lex_inline_op(id->c_str(), id->len());
        if (!op) {
            lambda = AssertGet(id, env, CHECK_OK);
        } else {
            return ApplyInline(op->id, expr->cdr(), env, ok);
        }
    } else {
        lambda = Eval(maybe_id, env, CHECK_OK);
    }

    // (lambda (a b c) (begin (a) (b) (b + c)))
    if (IsLambda(lambda)) {
        env->EnterScope(lambda)
        BindArguments(Cdar(lambda, env), expr->cdr(), env, ok);
        Object *ev = nullptr;
        if (*ok) {
            ev = Eval(Cddr(lambda, env), env, ok);
        }
        env->LeaveScope(*ok);
        return ev;
    } else {
        // TODO:
    }
}

Object *Evaluate::ApplyInline(int id, Pair *args, Env *env, bool *) {
    switch (id) {
    case OP_PLUS {
            auto lhs = EnsureNativeNumber(Car(args), env, CHECK_OK);
            auto rhs = EnsureNativeNumber(Cdar(args), env, CHECK_OK);

            return env->Number(lhs + rhs);
        } break;

    case OP_LET: {
            auto name = AssertId(Car(args), CHECK_OK);
            auto val  = Apply(Cdar(args), CHECK_OK);

            PutToScope(name, val, CHECK_OK);
            return val;
        } break;

    case OP_BEGIN: {
            Object *ev = env->Undef();
            auto more = args;
            while (!more->empty()) {
                ev = Eval(more->car(), env, CHECK_OK);
                more = more->cdr();
            }
            return ev;
        } break;

    case OP_DEF: {

        } break;
    }
}

NativeNumber Evaluate::EnsureNativeNumber(Object *obj, Env *env, bool *ok) {
    auto maybe_num = Eval(obj, env, CHECK_OK);
    if (!maybe_num->is_number()) {
        *ok = false;
        return 0;
    }

    return static_cast<Number*>(maybe_num)->native();
}
