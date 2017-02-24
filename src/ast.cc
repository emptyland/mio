#include "ast.h"
#include "glog/logging.h"

namespace mio {

const OperatorPriority kOpsPriority[] = {
#define OpsPriority_GLOBAL_DEFINE(name, left_proi, right_proi, token) \
    { .left = left_proi, .right = right_proi, },
    DEFINE_OPS(OpsPriority_GLOBAL_DEFINE)
#undef  OpsPriority_GLOBAL_DEFINE
};

#define AstNode_ACCEPT_METHOD(name)      \
    void name::Accept(AstVisitor *v) {   \
        v->Visit##name(this);            \
    }
DEFINE_AST_NODES(AstNode_ACCEPT_METHOD)
#undef AstNode_ACCEPT_METHOD

Operator TokenToBinaryOperator(Token token) {
    switch (token) {
#define TokenToBinaryOperator_SWITCH_CASE(name, left_proi, right_proi, token) \
    case token: return OP_##name;
        DEFINE_SIMPLE_ARITH_OPS(TokenToBinaryOperator_SWITCH_CASE)
        DEFINE_BIT_OPS(TokenToBinaryOperator_SWITCH_CASE)
        DEFINE_LOGIC_OPS(TokenToBinaryOperator_SWITCH_CASE)
        DEFINE_CONDITION_OPS(TokenToBinaryOperator_SWITCH_CASE)
#undef TokenToBinaryOperator_SWITCH_CASE
        default: return OP_NOT_BINARY;
    }
}

Operator TokenToUnaryOperator(Token token) {
    switch (token) {
#define TokenToUnaryOperator_SWITCH_CASE(name, left_proi, right_proi, token) \
    case token: return OP_##name;
        DEFINE_UNARY_OPS(TokenToUnaryOperator_SWITCH_CASE)
#undef TokenToUnaryOperator_SWITCH_CASE
        default: return OP_NOT_UNARY;
    }
}

const OperatorPriority *GetOperatorPriority(Operator op) {
    auto index = static_cast<int>(op);
    DCHECK_GE(index, 0);
    DCHECK_LT(index, MAX_OP);

    return &kOpsPriority[index];
}

bool Expression::is_lval() const {
    if (IsSymbol() || IsFieldAccessing() || IsCall() || IsAssignment()) {
        return true;
    } else {
        return false;
    }
}

} // namespace mio
