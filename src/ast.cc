#include "ast.h"

namespace mio {

#define AstNode_ACCEPT_METHOD(name)      \
    void name::Accept(AstVisitor *v) {   \
        v->Visit##name(this);            \
    }
DEFINE_AST_NODES(AstNode_ACCEPT_METHOD)
#undef AstNode_ACCEPT_METHOD

} // namespace mio
