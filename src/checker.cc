#include "checker.h"
#include "scopes.h"
#include "types.h"
#include "ast.h"
#include "text-output-stream.h"
#include <stack>
#include <vector>

namespace mio {

class FunctionInfoScope;
class LoopInfoScope;

#define CHECK_OK &ok); if (!ok) { return; } ((void)0

class ScopeHolder {
public:
    ScopeHolder(Scope *new_scope, Scope **current)
        : saved_scope_(*DCHECK_NOTNULL(current))
        , current_(current) {
        DCHECK_NE(new_scope, *current_);
        *current_ = DCHECK_NOTNULL(new_scope);
    }

    ~ScopeHolder() {
        *current_ = saved_scope_;
    }

    DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeHolder)
private:
    Scope *saved_scope_;
    Scope **current_;
}; // class ScopeHolder


class FunctionInfoScope {
public:
    FunctionInfoScope(FunctionInfoScope **current,
                      ZoneVector<Variable *> *up_values,
                      Scope *fn_scope,
                      Zone *zone)

        : saved_(*DCHECK_NOTNULL(current))
        , current_(current)
        , up_values_(DCHECK_NOTNULL(up_values))
        , fn_scope_(DCHECK_NOTNULL(fn_scope))
        , zone_(DCHECK_NOTNULL(zone)) {
        DCHECK_NE(this, *current_);
        *current_ = this;
        types_ = new (zone_) Union::TypeMap(zone_);
    }

    ~FunctionInfoScope() {
        *current_ = saved_;
        if (types_) {
            types_->~ZoneHashMap();
            zone_->Free(types_);
        }
    }

    DEF_PTR_GETTER(Scope, fn_scope)
    DEF_PTR_PROP_RW(LoopInfoScope, loop_info_scope)

    void Apply(Type *type) { types_->Put(type->GenerateId(), type); }

    Union::TypeMap *ReleaseTypes() {
        auto types = types_;
        types_ = nullptr;
        return types;
    }

    Type *GenerateType(TypeFactory *factory) {
        if (types_->is_empty()) {
            return factory->GetVoid();
        }
        if (types_->size() == 1) {
            Union::TypeMap::Iterator iter(types_);
            iter.Init();
            DCHECK(iter.HasNext());
            return iter->value();
        }

        return factory->GetUnion(ReleaseTypes());
    }

    Variable *CreateUpValue(RawStringRef name, Variable *for_link, int position) {
        auto upval = fn_scope_->Declare(name, for_link, position);
        if (!upval) {
            return nullptr;
        }
        DCHECK_NOTNULL(up_values_)->Add(upval);
        return upval;
    }

private:
    Union::TypeMap *types_;
    FunctionInfoScope *saved_;
    FunctionInfoScope **current_;
    Scope *fn_scope_;
    Zone *zone_;
    ZoneVector<Variable *> *up_values_;
    LoopInfoScope *loop_info_scope_ = nullptr;
}; // class FunctionInfoScope


class LoopInfoScope {
public:
    LoopInfoScope(LoopInfoScope **current, FunctionInfoScope *fn, Loop *loop)
        : saved_(*DCHECK_NOTNULL(current))
        , current_(current)
        , fn_(fn)
        , loop_(DCHECK_NOTNULL(loop)) {
        *current_ = this;
        if (fn_) {
            fn_->set_loop_info_scope(this);
        }
    }

    ~LoopInfoScope() {
        *current_ = saved_;
        if (fn_) {
            fn_->set_loop_info_scope(saved_);
        }
    }

    DEF_PTR_GETTER(Loop, loop)

private:
    LoopInfoScope *saved_;
    LoopInfoScope **current_;
    FunctionInfoScope *fn_;
    Loop *loop_;
}; // class LoopInfoScope

#define ACCEPT_REPLACE_EXPRESSION(node, field) \
    (node)->field()->Accept(this); \
    if (has_error()) { \
        return; \
    } \
    if (has_analysis_expression()) { \
        auto expr = AnalysisExpression(); \
        if (expr->IsReference()) { \
            auto var = expr->AsReference()->variable(); \
            if (var->scope()->type() != MODULE_SCOPE && \
                var->scope()->type() != UNIT_SCOPE && \
                var->declaration()->position() > node->position()) { \
                ThrowError(node, "symbol \'%s\' is not found.", \
                var->declaration()->name()->c_str()); \
                return; \
            } \
        } \
        (node)->set_##field(AnalysisExpression()); \
        PopAnalysisExpression(); \
    } (void)0

#define ACCEPT_REPLACE_EXPRESSION_I(node, field, idx) \
    node->field(idx)->Accept(this); \
    if (has_error()) { \
        return; \
    } \
    if (has_analysis_expression()) { \
        auto expr = AnalysisExpression(); \
        if (expr->IsReference()) { \
            auto var = expr->AsReference()->variable(); \
            if (var->scope()->type() != MODULE_SCOPE && \
                var->scope()->type() != UNIT_SCOPE && \
                var->declaration()->position() > node->position()) { \
                ThrowError(node, "symbol \'%s\' is not found.", \
                var->declaration()->name()->c_str()); \
                return; \
            } \
        } \
        node->set_##field(idx, AnalysisExpression()); \
        PopAnalysisExpression(); \
    } (void)0

////////////////////////////////////////////////////////////////////////////////
//// class CheckingAstVisitor
////////////////////////////////////////////////////////////////////////////////
class CheckingAstVisitor : public DoNothingAstVisitor {
public:
    CheckingAstVisitor(TypeFactory *types,
                       RawStringRef unit_name,
                       PackageImporter::ImportList *import_list,
                       Scope *global,
                       Scope *scope,
                       Checker *checker,
                       Zone *zone)
        : types_(DCHECK_NOTNULL(types))
        , unit_name_(DCHECK_NOTNULL(unit_name))
        , import_list_(DCHECK_NOTNULL(import_list))
        , global_(DCHECK_NOTNULL(global))
        , scope_(DCHECK_NOTNULL(scope))
        , checker_(DCHECK_NOTNULL(checker))
        , factory_(new AstNodeFactory(DCHECK_NOTNULL(zone)))
        , zone_(DCHECK_NOTNULL(zone)) {}

    virtual void VisitValDeclaration(ValDeclaration *node) override;
    virtual void VisitVarDeclaration(VarDeclaration *node) override;
    virtual void VisitCall(Call *node) override;
    virtual void VisitBuiltinCall(BuiltinCall *node) override;
    virtual void VisitUnaryOperation(UnaryOperation *node) override;
    virtual void VisitAssignment(Assignment *node) override;
    virtual void VisitBinaryOperation(BinaryOperation *node) override;
    virtual void VisitSymbol(Symbol *node) override;
    virtual void VisitSmiLiteral(SmiLiteral *node) override;
    virtual void VisitFloatLiteral(FloatLiteral *node) override;
    virtual void VisitStringLiteral(StringLiteral *node) override;
    virtual void VisitIfOperation(IfOperation *node) override;
    virtual void VisitBlock(Block *node) override;
    virtual void VisitWhileLoop(WhileLoop *node) override;
    virtual void VisitForeachLoop(ForeachLoop *node) override;
    virtual void VisitReturn(Return *node) override;
    virtual void VisitBreak(Break *node) override;
    virtual void VisitContinue(Continue *node) override;
    virtual void VisitFunctionDefine(FunctionDefine *node) override;
    virtual void VisitFunctionLiteral(FunctionLiteral *node) override;
    virtual void VisitArrayInitializer(ArrayInitializer *node) override;
    virtual void VisitMapInitializer(MapInitializer *node) override;
    virtual void VisitFieldAccessing(FieldAccessing *node) override;
    virtual void VisitTypeTest(TypeTest *node) override;
    virtual void VisitTypeCast(TypeCast *node) override;
    virtual void VisitTypeMatch(TypeMatch *node) override;
    virtual void VisitVariable(Variable *node) override;
    virtual void VisitReference(Reference *node) override;

    virtual void VisitElement(Element *node) override {
        DLOG(FATAL) << "noreached!";
    }
    virtual void VisitPair(Pair *node) override {
        DLOG(FATAL) << "noreached!";
    }

    Type *AnalysisType() { return type_stack_.top(); };

    void PopEvalType() {
        if (!type_stack_.empty()) {
            type_stack_.pop();
        }
    }

    Expression *AnalysisExpression() { return expr_stack_.top(); }

    bool has_analysis_expression() const { return !expr_stack_.empty(); }

    void PushAnalysisExpression(Expression *expression) {
        expr_stack_.push(expression);
    }

    void PopAnalysisExpression() { expr_stack_.pop(); }

    bool has_error() { return checker_->has_error(); }

private:
    void CheckFunctionCall(FunctionPrototype *proto, Call *node);
    void CheckMapAccessor(Map *map, Call *node);
    void CheckArrayAccessorOrMakeSlice(Type *callee, Call *node);

    bool AcceptOrReduceFunctionLiteral(AstNode *node, Type *target_ty,
                                       FunctionLiteral *rval);

    void SetEvalType(Type *type) {
        if (!type_stack_.empty()) {
            type_stack_.pop();
        }
        type_stack_.push(type);
    }

    void PushEvalType(Type *type) { type_stack_.push(type); }

    void ThrowError(AstNode *node, const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        checker_->VThrowError(unit_name_, node, fmt, ap);
        va_end(ap);
    }

    TypeFactory *types_;
    RawStringRef unit_name_;
    PackageImporter::ImportList *import_list_;
    Scope *global_;
    Scope *scope_;
    Checker *checker_;
    std::stack<Type *> type_stack_;
    std::stack<Expression *> expr_stack_;
    FunctionInfoScope *fn_info_scope_ = nullptr;
    LoopInfoScope *loop_info_scope_ = nullptr;
    std::unique_ptr<AstNodeFactory> factory_;
    Zone *zone_;
};


/*virtual*/ void CheckingAstVisitor::VisitValDeclaration(ValDeclaration *node) {
    if (node->has_initializer()) {
        ACCEPT_REPLACE_EXPRESSION(node, initializer);
        node->set_initializer_type(AnalysisType());
    }

    if (node->type() == types_->GetUnknown()) {
        DCHECK_NOTNULL(node->initializer());
        node->set_type(AnalysisType());
    } else {
        if (node->type()->MustBeInitialized() && !node->has_initializer()) {
            ThrowError(node, "type %s need initializer.",
                       node->type()->ToString().c_str());
            return;
        }

        if (node->has_initializer() &&
            !node->type()->CanAcceptFrom(AnalysisType())) {
            ThrowError(node, "val %s can not accept initializer type",
                       node->name()->c_str());
            return;
        }
    }
    SetEvalType(types_->GetVoid());
}

/*virtual*/ void CheckingAstVisitor::VisitVarDeclaration(VarDeclaration *node) {
    if (node->has_initializer()) {
        ACCEPT_REPLACE_EXPRESSION(node, initializer);
        node->set_initializer_type(AnalysisType());
    }

    if (node->type() == types_->GetUnknown()) {
        DCHECK_NOTNULL(node->initializer());
        node->set_type(AnalysisType());
    } else {
        if (node->type()->MustBeInitialized() && !node->has_initializer()) {
            ThrowError(node, "type %s need initializer.",
                       node->type()->ToString().c_str());
            return;
        }

        if (node->has_initializer() &&
            !node->type()->CanAcceptFrom(AnalysisType())) {
            ThrowError(node, "var %s can not accept initializer type",
                       node->name()->c_str());
            return;
        }
    }
    SetEvalType(types_->GetVoid());
}

/*virtual*/ void CheckingAstVisitor::VisitCall(Call *node) {
    ACCEPT_REPLACE_EXPRESSION(node, expression);
    auto callee_ty = AnalysisType();
    PopEvalType();

    node->set_callee_type(callee_ty);
    if (callee_ty->IsFunctionPrototype()) {
        CheckFunctionCall(callee_ty->AsFunctionPrototype(), node);
    } else if (callee_ty->IsMap()) {
        CheckMapAccessor(callee_ty->AsMap(), node);
    } else if (callee_ty->IsSlice() || callee_ty->IsArray()) {
        CheckArrayAccessorOrMakeSlice(callee_ty, node);
    } else {
        ThrowError(node, "this type can not be call.");
    }
}

/*virtual*/ void CheckingAstVisitor::VisitBuiltinCall(BuiltinCall *node) {
    switch (node->code()) {
        case BuiltinCall::LEN: {
            if (node->argument_size() != 1) {
                ThrowError(node, "len: incorrect number of operands, expected %d, unexpected 1.",
                           node->argument_size());
                return;
            }

            auto arg = node->argument(0);
            ACCEPT_REPLACE_EXPRESSION(arg, value);
            if (!AnalysisType()->IsMap() &&
                !AnalysisType()->IsArray() &&
                !AnalysisType()->IsSlice() &&
                !AnalysisType()->IsString()) {

                ThrowError(node, "len: incorrent type of operands, expected %s.",
                           AnalysisType()->ToString().c_str());
                return;
            }
            arg->set_value_type(AnalysisType());
            PopEvalType();
            PushEvalType(types_->GetInt());
        } break;

        case BuiltinCall::ADD: {
            if (node->argument_size() != 2) {
                ThrowError(node, "add: incorrect number of operands, expected %d, unexpected 2.",
                           node->argument_size());
                return;
            }

            auto container = node->argument(0);
            ACCEPT_REPLACE_EXPRESSION(container, value);
            if (!AnalysisType()->IsArray()) {
                ThrowError(container, "add: incorrent type of operands, expected %s.",
                           AnalysisType()->ToString().c_str());
                return;
            }
            container->set_value_type(AnalysisType());
            PopEvalType();

            auto element = node->argument(1);
            ACCEPT_REPLACE_EXPRESSION(element, value);
            if (!container->value_type()->AsArray()->element()->CanAcceptFrom(AnalysisType())) {
                ThrowError(element, "add: type %s can not add to %s.",
                           AnalysisType()->ToString().c_str(),
                           container->value_type()->ToString().c_str());
                return;
            }
            element->set_value_type(AnalysisType());
            PopEvalType();

            PushEvalType(container->value_type());
        } break;

        case BuiltinCall::DELETE: {
            if (node->argument_size() != 2) {
                ThrowError(node, "delete: incorrect number of operands, expected %d, unexpected 2.",
                           node->argument_size());
                return;
            }

            auto container = node->argument(0);
            ACCEPT_REPLACE_EXPRESSION(container, value);
            if (!AnalysisType()->IsMap()) {
                ThrowError(container, "delete: incorrent type of operands, expected %s.",
                           AnalysisType()->ToString().c_str());
                return;
            }
            container->set_value_type(AnalysisType());
            PopEvalType();

            auto key = node->argument(1);
            ACCEPT_REPLACE_EXPRESSION(key, value);
            if (!container->value_type()->AsArray()->element()->CanAcceptFrom(AnalysisType())) {
                ThrowError(key, "delete: key type %s can not delete from %s.",
                           AnalysisType()->ToString().c_str(),
                           container->value_type()->ToString().c_str());
                return;
            }
            key->set_value_type(AnalysisType());
            PopEvalType();

            PushEvalType(types_->GetI8());
        } break;

        default:
            ThrowError(node, "builtin call not support yet.");
            break;
    }
}

/*virtual*/ void CheckingAstVisitor::VisitUnaryOperation(UnaryOperation *node) {
    ACCEPT_REPLACE_EXPRESSION(node, operand);
    auto type = AnalysisType();
    switch (node->op()) {
        case OP_MINUS:
            if (!AnalysisType()->is_numeric()) {
                ThrowError(node, "`-' operator only accept numeric type.");
            }
            break;

        case OP_BIT_INV:
            if (!AnalysisType()->IsIntegral()) {
                ThrowError(node, "`~' operator only accept interal type.");
            }
            break;

        case OP_NOT:
            if (AnalysisType() != types_->GetI1()) {
                ThrowError(node, "`not' operator only accept bool type.");
            }
            PopEvalType();
            PushEvalType(types_->GetI1());
            break;

        default:
            DLOG(FATAL) << "noreached!";
            break;
    }
    node->set_operand_type(type);
    // keep type, DO NOT POP EVAL TYPE
}

/*virtual*/ void CheckingAstVisitor::VisitAssignment(Assignment *node) {
    ACCEPT_REPLACE_EXPRESSION(node, target);
    auto target_ty = AnalysisType();
    PopEvalType();

    if (!node->target()->is_lval()) {
        ThrowError(node, "assignment target is not a lval.");
        return;
    }

    if (node->rval()->IsFunctionLiteral() &&
        !AcceptOrReduceFunctionLiteral(node, target_ty,
                                       node->rval()->AsFunctionLiteral())) {
        return;
    }
    ACCEPT_REPLACE_EXPRESSION(node, rval);
    node->set_rval_type(AnalysisType());
    if (!target_ty->CanAcceptFrom(AnalysisType())) {
        ThrowError(node, "assignment taget can not accept rval type. %s vs %s",
                   target_ty->ToString().c_str(),
                   AnalysisType()->ToString().c_str());
        return;
    }
    PopEvalType();
    PushEvalType(types_->GetVoid());
}

/*virtual*/ void CheckingAstVisitor::VisitBinaryOperation(BinaryOperation *node) {
    ACCEPT_REPLACE_EXPRESSION(node, lhs);
    auto lhs_ty = AnalysisType();
    PopEvalType();

    ACCEPT_REPLACE_EXPRESSION(node, rhs);
    auto rhs_ty = AnalysisType();
    PopEvalType();

    switch(node->op()) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
            if (lhs_ty->GenerateId() != rhs_ty->GenerateId()) {
                ThrowError(node, "operator: `%s' has different type of operands.",
                           GetOperatorText(node->op()));
            }
            if (!lhs_ty->is_numeric()) {
                ThrowError(node, "operator: `%s' only accept numeric type.",
                           GetOperatorText(node->op()));
            }
            PushEvalType(lhs_ty);
            break;

        case OP_BIT_OR:
        case OP_BIT_AND:
        case OP_BIT_XOR:
            if (lhs_ty->GenerateId() != rhs_ty->GenerateId()) {
                ThrowError(node, "operator: `%s' has different type of operands.",
                           GetOperatorText(node->op()));
            }
            if (!lhs_ty->IsIntegral()) {
                ThrowError(node, "operator: `%s' only accept numeric type.",
                           GetOperatorText(node->op()));
            }
            PushEvalType(lhs_ty);
            break;

        case OP_LSHIFT:
        case OP_RSHIFT_A:
        case OP_RSHIFT_L:
            if (!lhs_ty->IsIntegral() || !rhs_ty->IsIntegral()) {
                ThrowError(node, "operator: `%s' only accept integral type.",
                           GetOperatorText(node->op()));
            }
            PushEvalType(lhs_ty);
            break;

        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            if (lhs_ty->GenerateId() != rhs_ty->GenerateId()) {
                ThrowError(node, "operator: `%s' has different type of operands.",
                           GetOperatorText(node->op()));
            }
            if (!lhs_ty->is_numeric() && !lhs_ty->IsString()) {
                ThrowError(node, "operator: `%s' only accept numeric and string type.",
                           GetOperatorText(node->op()));
            }
            PushEvalType(types_->GetI1());
            break;

        case OP_OR:
        case OP_AND:
            if (lhs_ty->GenerateId() != rhs_ty->GenerateId()) {
                ThrowError(node, "operator: `%s' has different type of operands.",
                           GetOperatorText(node->op()));
            }
            if (lhs_ty != types_->GetI1()) {
                ThrowError(node, "operator: `%s' only accept bool type.",
                           GetOperatorText(node->op()));
            }
            PushEvalType(types_->GetI1());
            break;

        case OP_STRCAT:
            PushEvalType(types_->GetString());
            break;

        default:
            DLOG(FATAL) << "noreached";
            break;
    }
    node->set_lhs_type(lhs_ty);
    node->set_rhs_type(rhs_ty);
}

/*virtual*/ void CheckingAstVisitor::VisitSymbol(Symbol *node) {
    Scope *scope = scope_;

    if (node->has_name_space()) {
        auto pair = import_list_->Get(node->name_space());
        if (!pair) {
            ThrowError(node, "pacakge: \'%s\' has not import yet.",
                       node->name_space()->c_str());
            return;
        }
        scope = global_->FindInnerScopeOrNull(node->name_space());
    }

    Scope *owned;
    auto var = scope->FindOrNullRecursive(node->name(), &owned);
    if (!var) {
        ThrowError(node, "symbol \'%s\' is not found", node->name()->c_str());
        return;
    }


    if (owned->is_local()) {
        auto curr = fn_info_scope_->fn_scope()->outter_scope();
        while (curr) {
            if (owned == curr) {
                // is up value.
                var = fn_info_scope_->CreateUpValue(node->name(), var,
                                                    node->position());
                break;
            }
            curr = curr->outter_scope();
        }
    }

    if (var->type() == types_->GetUnknown()) {
        if (var->scope() == scope_) {
            ThrowError(node, "symbol \'%s\', its' type unknown.",
                       node->name()->c_str());
            return;
        }

        ScopeHolder holder(var->scope(), &scope_);
        var->declaration()->Accept(this);
        PopEvalType();

        if (var->type() == types_->GetUnknown()) {
            ThrowError(node, "symbol \'%s\', its' type unknown.",
                       node->name()->c_str());
            return;
        }
    }

    PushAnalysisExpression(factory_->CreateReference(var, node->position()));
    PushEvalType(var->type());
}

/*virtual*/ void CheckingAstVisitor::VisitSmiLiteral(SmiLiteral *node) {
    switch (node->bitwide()) {
        case 1:
            PushEvalType(types_->GetI1());
            break;

        case 8:
            PushEvalType(types_->GetI8());
            break;

        case 16:
            PushEvalType(types_->GetI16());
            break;

        case 32:
            PushEvalType(types_->GetI32());
            break;

        case 64:
            PushEvalType(types_->GetI64());
            break;

        default:
            DLOG(FATAL) << "noreached: bitwide = " << node->bitwide();
            break;
    }
}

void CheckingAstVisitor::VisitFloatLiteral(FloatLiteral *node) {
    switch (node->bitwide()) {
        case 32:
            PushEvalType(types_->GetF32());
            break;

        case 64:
            PushEvalType(types_->GetF64());
            break;

        default:
            DLOG(FATAL) << "noreached: bitwide = " << node->bitwide();
            break;
    }
}

/*virtual*/ void CheckingAstVisitor::VisitStringLiteral(StringLiteral *node) {
    PushEvalType(types_->GetString());
}

/*virtual*/ void CheckingAstVisitor::VisitIfOperation(IfOperation *node) {
    ACCEPT_REPLACE_EXPRESSION(node, condition);
    PopEvalType();

    ACCEPT_REPLACE_EXPRESSION(node, then_statement);
    node->set_then_type(AnalysisType());
    PopEvalType();

    node->set_else_type(types_->GetVoid());
    if (node->has_else()) {
        ACCEPT_REPLACE_EXPRESSION(node, else_statement);
        node->set_else_type(AnalysisType());
        PopEvalType();
    }

    if (node->then_type()->GenerateId() != node->else_type()->GenerateId()) {
        Type *types[2] = {node->then_type(), node->else_type()};
        PushEvalType(types_->MergeToFlatUnion(types, 2));
    } else {
        PushEvalType(node->then_type());
    }
}

/*virtual*/ void CheckingAstVisitor::VisitBlock(Block *node) {
    if (node->statements()->is_empty()) {
        PushEvalType(types_->GetVoid());
        return;
    }

    ScopeHolder holder(node->scope(), &scope_);
    for (int i = 0; i < node->statement_size(); ++i) {
        ACCEPT_REPLACE_EXPRESSION_I(node, statement, i);
        if (i < node->statement_size() - 1) {
            PopEvalType();
        }
    }
    // keep the last expression type for assignment type.
}

/*virtual*/ void CheckingAstVisitor::VisitWhileLoop(WhileLoop *node) {
    LoopInfoScope loop_holder(&loop_info_scope_, fn_info_scope_, node);

    ACCEPT_REPLACE_EXPRESSION(node, condition);
    auto cond_type = AnalysisType();
    PopEvalType();

    if (cond_type != types_->GetI1()) {
        ThrowError(node, "this type: %s is not bool for while condition",
                   AnalysisType()->ToString().c_str());
        return;
    }
    ACCEPT_REPLACE_EXPRESSION(node, body);
    PopEvalType();
    PushEvalType(types_->GetVoid());
}

/*virtual*/ void CheckingAstVisitor::VisitForeachLoop(ForeachLoop *node) {
    LoopInfoScope loop_holder(&loop_info_scope_, fn_info_scope_, node);

    ACCEPT_REPLACE_EXPRESSION(node, container);
    auto container_type = AnalysisType();
    if (container_type->CanNotIteratable()) {
        ThrowError(node, "this type: %s can not be foreach",
                   AnalysisType()->ToString().c_str());
        return;
    }
    PopEvalType();

    if (container_type->IsMap()) {
        auto map = container_type->AsMap();

        if (node->has_key()) {
            node->key()->set_type(map->key());
        }
        node->value()->set_type(map->value());
    } else if (container_type->IsArray() || container_type->IsSlice()) {
        auto array = static_cast<Array *>(container_type);

        if (node->has_key()) {
            node->key()->set_type(types_->GetInt());
        }
        node->value()->set_type(array->element());
    }
    node->set_container_type(container_type);
    ACCEPT_REPLACE_EXPRESSION(node, body);
    PopEvalType();
    PushEvalType(types_->GetVoid());
}

/*virtual*/ void CheckingAstVisitor::VisitReturn(Return *node) {
    auto func_scope = DCHECK_NOTNULL(scope_->FindOuterScopeOrNull(FUNCTION_SCOPE));
    if (func_scope->function()->function_literal()->is_assignment()) {
        ThrowError(node, "assignment function don not need return.");
        return;
    }

    if (!fn_info_scope_) {
        ThrowError(node, "return out of function scope.");
        return;
    }

    if (node->has_return_value()) {
        ACCEPT_REPLACE_EXPRESSION(node, expression);
        if (AnalysisType()->IsVoid()) {
            ThrowError(node, "return void type.");
            return;
        }
        DCHECK_NOTNULL(fn_info_scope_)->Apply(AnalysisType());
        PopEvalType();
    } else {
        DCHECK_NOTNULL(fn_info_scope_)->Apply(types_->GetVoid());
    }
    PushEvalType(types_->GetVoid());
}

/*virtual*/ void CheckingAstVisitor::VisitBreak(Break *node) {
    auto current_loop = fn_info_scope_ ? fn_info_scope_->loop_info_scope() :
                        loop_info_scope_;
    if (!current_loop) {
        ThrowError(node, "break out of loop block.");
        return;
    }
    PushEvalType(types_->GetVoid());
}

/*virtual*/ void CheckingAstVisitor::VisitContinue(mio::Continue *node) {
    auto current_loop = fn_info_scope_
                      ? fn_info_scope_->loop_info_scope()
                      : loop_info_scope_;
    if (!current_loop) {
        ThrowError(node, "continue out of loop block.");
        return;
    }
    PushEvalType(types_->GetVoid());
}

/*virtual*/ void CheckingAstVisitor::VisitFunctionDefine(FunctionDefine *node) {
    auto proto = node->function_literal()->prototype();
    if (node->is_native()) {
        if (node->function_literal()->has_body()) {
            ThrowError(node, "function: %s, native function don't need body",
                       node->name()->c_str());
            return;
        }

        if (proto->return_type()->IsUnknown()) {
            ThrowError(node, "function: %s, native function has unknown "
                       "return type",
                       node->name()->c_str());
            return;
        }

        return;
    }
    if (!node->function_literal()->has_body()) {
        ThrowError(node, "function: %s, non native function need body",
                   node->name()->c_str());
        return;
    }

    VisitFunctionLiteral(node->function_literal());
    SetEvalType(types_->GetVoid());
}

/*virtual*/ void CheckingAstVisitor::VisitFunctionLiteral(FunctionLiteral *node) {
    ScopeHolder holder(node->scope(), &scope_);
    FunctionInfoScope info(&fn_info_scope_, node->mutable_up_values(),
                           node->scope(), zone_);

    ACCEPT_REPLACE_EXPRESSION(node, body);

    Type *return_type = nullptr;
    if (node->is_assignment()) {
        return_type = AnalysisType();
    } else {
        return_type = info.GenerateType(types_);
    }
    PopEvalType();

    auto proto = node->prototype();
    if (proto->return_type()->IsUnknown()) {
        proto->set_return_type(return_type);
    } else {
        if (!proto->return_type()->CanAcceptFrom(return_type)) {
            ThrowError(node, "function: %s, can not accept return type. %s vs %s",
                       node->scope()->name()->c_str(),
                       proto->return_type()->ToString().c_str(),
                       return_type->ToString().c_str());
            return;
        }
    }
    PushEvalType(proto);
}

void CheckingAstVisitor::VisitArrayInitializer(ArrayInitializer *node) {
    auto array_type = node->array_type();
    if (array_type->element()->IsUnknown() && node->element_size() == 0) {
        ThrowError(node, "array initializer has unknown element type.");
        return;
    }

    auto value_types = new (types_->zone()) ZoneHashMap<int64_t, Type *>(types_->zone());
    for (int i = 0; i < node->element_size(); ++i) {
        auto element = node->element(i);
        ACCEPT_REPLACE_EXPRESSION(element, value);
        auto value = AnalysisType();
        PopEvalType();

        if (!array_type->element()->IsUnknown() &&
            !array_type->element()->CanNotAcceptFrom(value)) {
            ThrowError(node->element(i), "array initializer element can accept expression, (%s vs %s)",
                       array_type->element()->ToString().c_str(),
                       value->ToString().c_str());
            return;
        }

        element->set_value_type(value);
        value_types->Put(value->GenerateId(), value);
    }

    if (array_type->element()->IsUnknown()) {
        DCHECK(value_types->is_not_empty());
        if (value_types->size() > 1) {
            array_type->set_element(types_->GetUnion(value_types));
        } else {
            array_type->set_element(value_types->first()->value());
        }
    }

    DCHECK(!array_type->element()->IsUnknown());
    PushEvalType(node->array_type());
}

/*virtual*/
void CheckingAstVisitor::VisitMapInitializer(MapInitializer *node) {
    auto map_type = node->map_type();
    if (map_type->key()->IsUnknown() || map_type->value()->IsUnknown()) {
        if (node->pairs()->is_empty()) {
            ThrowError(node, "map initializer has unknwon key and value's type");
            return;
        }
    } else if (map_type->key()->CanNotBeKey()) {
        ThrowError(node, "type %s can not be map key.",
                   map_type->key()->ToString().c_str());
        return;
    }

    auto value_types = new (types_->zone()) ZoneHashMap<int64_t, Type *>(types_->zone());
    for (int i = 0; i < node->pairs()->size(); ++i) {
        auto pair = node->pairs()->At(i);
        ACCEPT_REPLACE_EXPRESSION(pair, key);
        auto key = AnalysisType();
        PopEvalType();

        if (map_type->key()->IsUnknown()) {
            map_type->set_key(key);
        } else if (map_type->key()->CanNotAcceptFrom(key)) {
            ThrowError(pair->key(), "map initializer key can accept expression, (%s vs %s)",
                       map_type->key()->ToString().c_str(),
                       key->ToString().c_str());
            return;
        }
        if (map_type->key()->CanNotBeKey()) {
            ThrowError(pair->key(), "type %s can not be map key.",
                       map_type->key()->ToString().c_str());
            return;
        }

        ACCEPT_REPLACE_EXPRESSION(pair, value);
        auto value = AnalysisType();
        PopEvalType();

        if (!map_type->value()->IsUnknown() &&
            !map_type->value()->CanAcceptFrom(value)) {
            ThrowError(pair->key(), "map initializer value can accept expression, (%s vs %s)",
                       map_type->value()->ToString().c_str(),
                       value->ToString().c_str());
            return;
        }
        pair->set_value_type(value);
        value_types->Put(value->GenerateId(), value);
    }

    if (map_type->value()->IsUnknown()) {
        DCHECK(value_types->is_not_empty());
        if (value_types->size() > 1) {
            map_type->set_value(types_->GetUnion(value_types));
        } else {
            map_type->set_value(value_types->first()->value());
        }
    }

    DCHECK(!map_type->key()->IsUnknown() &&
           !map_type->value()->IsUnknown());
    PushEvalType(node->map_type());
}

/*virtual*/
void CheckingAstVisitor::VisitFieldAccessing(FieldAccessing *node) {
    ACCEPT_REPLACE_EXPRESSION(node, expression);
    auto type = AnalysisType();
    PopEvalType();
    node->set_callee_type(type);

    if (type->IsMap()) {
        auto map = type->AsMap();
        if (!map->key()->IsString()) {
            ThrowError(node, "map key type is not string, can not use .%s .",
                       node->field_name()->c_str());
            return;
        }

        Type *types[] = {types_->GetVoid(), map->value()};
        PushEvalType(types_->MergeToFlatUnion(types, arraysize(types)));
    } else {
        ThrowError(node, "this type(%s) can not use .%s .",
                   type->ToString().c_str(),
                   node->field_name()->c_str());
    }
    // TODO: other types:
}

/*virtual*/
void CheckingAstVisitor::VisitTypeTest(TypeTest *node) {
    ACCEPT_REPLACE_EXPRESSION(node, expression);
    auto type = AnalysisType();
    PopEvalType();

    if (type->IsUnion()) {
        if (!type->AsUnion()->CanBe(node->type())) {
            ThrowError(node, "union(%s) impossible to be %s",
                       type->ToString().c_str(),
                       node->type()->ToString().c_str());
        }
    } else {
        ThrowError(node, "this type(%s) can not use `is' operator.",
                   type->ToString().c_str());
    }
    PushEvalType(types_->GetI1());
}

/*virtual*/
void CheckingAstVisitor::VisitTypeCast(TypeCast *node) {
    ACCEPT_REPLACE_EXPRESSION(node, expression);
    auto type = AnalysisType();
    PopEvalType();

    if (type->is_numeric()) {
        if (!node->type()->is_numeric()) {
            ThrowError(node, "this type(%s) can not cast to %s.",
                       type->ToString().c_str(),
                       node->type()->ToString().c_str());
        }
    } else if (type->IsUnion()) {
        if (!type->AsUnion()->CanBe(node->type())) {
            ThrowError(node, "union(%s) impossible to be %s",
                       type->ToString().c_str(),
                       node->type()->ToString().c_str());
        }
    } else {
        ThrowError(node, "this type(%s) can not cast to %s.",
                   type->ToString().c_str(),
                   node->type()->ToString().c_str());
    }

    node->set_original(type);
    PushEvalType(node->type());
}

void CheckingAstVisitor::VisitTypeMatch(TypeMatch *node) {
    ACCEPT_REPLACE_EXPRESSION(node, target);
    auto type = AnalysisType();
    PopEvalType();

    if (!type->IsUnion()) {
        ThrowError(node, "this type(%s) can not match.", type->ToString().c_str());
        return;
    }

    AstNode *else_case = nullptr;
    std::set<int64_t> unique_type;
    auto uni = type->AsUnion();
    for (int i = 0; i < node->match_case_size(); ++i) {
        auto match_case = node->match_case(i);

        ScopeHolder holder(match_case->scope(), &scope_);
        ACCEPT_REPLACE_EXPRESSION(match_case, body);
        if (match_case->is_else_case()) {
            else_case = match_case;
            continue;
        }

        auto pattern = match_case->cast_pattern()->type();
        if (unique_type.find(pattern->GenerateId())
            != unique_type.end()) {
            ThrowError(match_case, "duplicated type %s in type match cases.",
                       pattern->ToString().c_str());
            return;
        }
        unique_type.insert(pattern->GenerateId());

        if (!uni->CanBe(pattern)) {
            ThrowError(match_case, "type %s never be matched in %s.",
                       pattern->ToString().c_str(),
                       type->ToString().c_str());
            return;
        }
    }

    if (else_case && unique_type.size() == uni->mutable_types()->size()) {
        ThrowError(else_case, "else case never be run.");
        return;
    }
}

void CheckingAstVisitor::VisitVariable(Variable *node) {
    DCHECK(!node->type()->IsUnknown());
    PushEvalType(node->type());
}

void CheckingAstVisitor::VisitReference(Reference *node) {
    DCHECK(!node->variable()->type()->IsUnknown());
    PushEvalType(node->variable()->type());
}

void CheckingAstVisitor::CheckFunctionCall(FunctionPrototype *proto, Call *node) {
    if (proto->paramter_size() != node->argument_size()) {
        ThrowError(node, "call argument number is not be accept (%d vs %d).",
                   node->argument_size(),
                   proto->paramter_size());
        return;
    }
    for (int i = 0; i < node->argument_size(); ++i) {
        auto arg   = node->argument(i);
        auto param = proto->paramter(i);

        if (arg->value()->IsFunctionLiteral() &&
            !AcceptOrReduceFunctionLiteral(node, param->param_type(),
                                           arg->value()->AsFunctionLiteral())) {
                return;
            }

        ACCEPT_REPLACE_EXPRESSION(arg, value);
        auto arg_ty = AnalysisType();
        PopEvalType();

        if (!param->param_type()->CanAcceptFrom(arg_ty)) {
            if (param->has_name()) {
                ThrowError(node, "call paramter: %s(%d) can not accpet this type. %s vs %s",
                           param->param_name()->c_str(), i,
                           param->param_type()->ToString().c_str(),
                           arg_ty->ToString().c_str());
            } else {
                ThrowError(node, "call paramter: (%d) can not accpet this type",
                           i);
            }
            return;
        }
        arg->set_value_type(arg_ty);
    }
    PushEvalType(proto->return_type());
}

void CheckingAstVisitor::CheckMapAccessor(Map *map, Call *node) {
    if (node->argument_size() != 1) {
        ThrowError(node, "incorrect arguments number of map calling.");
        return;
    }

    // Getting
    auto key = node->argument(0);
    ACCEPT_REPLACE_EXPRESSION(key, value);

    auto key_ty = AnalysisType();
    PopEvalType();

    if (!map->key()->CanAcceptFrom(key_ty)) {
        ThrowError(node->argument(0),
                   "map key can not accept input type, (%s vs %s)",
                   map->key()->ToString().c_str(),
                   key_ty->ToString().c_str());
        return;
    }

    Type *types[] = {map->value(), types_->GetVoid()};
    PushEvalType(types_->MergeToFlatUnion(types, arraysize(types)));
}

void CheckingAstVisitor::CheckArrayAccessorOrMakeSlice(Type *callee, Call *node) {
    DCHECK(callee->IsArray() || callee->IsSlice());
    auto array = static_cast<Array *>(callee);

    if (node->argument_size() == 1) {
        // Getting
        auto index = node->argument(0);
        ACCEPT_REPLACE_EXPRESSION(index, value);

        auto index_ty = AnalysisType();
        PopEvalType();

        if (!index_ty->IsIntegral()) {
            ThrowError(node->argument(0), "array/slice index need integral number.");
            return;
        }
        if (index_ty != types_->GetInt()) {
            auto arg = node->argument(0);
            auto cast = factory_->CreateTypeCast(arg->value(), types_->GetInt(),
                                                 arg->position());
            arg->set_value(cast);
        }
        PushEvalType(array->element());
    } else if (node->argument_size() == 2) {
        auto begin = node->argument(0);
        ACCEPT_REPLACE_EXPRESSION(begin, value);
        auto begin_ty = AnalysisType();
        PopEvalType();

        if (!begin_ty->IsIntegral()) {
            ThrowError(node->argument(0), "make slice need integral number "
                       "\"begin\" position.");
            return;
        }
        if (begin_ty != types_->GetInt()) {
            auto arg = node->argument(0);
            auto cast = factory_->CreateTypeCast(arg->value(), types_->GetInt(),
                                                 arg->position());
            arg->set_value(cast);
        }

        auto size = node->argument(1);
        ACCEPT_REPLACE_EXPRESSION(size, value);
        auto size_ty = AnalysisType();
        PopEvalType();

        if (!size_ty->IsIntegral()) {
            ThrowError(node->argument(1), "make slice need integral number "
                       "\"size\".");
            return;
        }
        if (size_ty != types_->GetInt()) {
            auto arg = node->argument(1);
            auto cast = factory_->CreateTypeCast(arg->value(), types_->GetInt(),
                                                 arg->position());
            arg->set_value(cast);
        }

        if (array->IsSlice()) {
            PushEvalType(array);
        } else {
            PushEvalType(types_->GetSlice(array->element()));
        }
    } else {
        ThrowError(node, "incorrect arguments number of array/slice calling.");
    }
}

bool CheckingAstVisitor::AcceptOrReduceFunctionLiteral(AstNode *node,
                                                       Type *target_ty,
                                                       FunctionLiteral *func) {
    auto rproto = func->prototype();

    if (!target_ty->IsFunctionPrototype()) {
        ThrowError(node, "target type is not function (%s)",
                   target_ty->ToString().c_str());
        return false;
    }
    auto lproto = target_ty->AsFunctionPrototype();
    auto scope = func->scope();
    if (rproto->paramters()->is_empty()) {
        for (int i = 0; i < lproto->paramter_size(); ++i) {
            auto lparam = lproto->paramter(i);
            auto rparam = types_->CreateParamter(TextOutputStream::sprintf("_%d", i + 1),
                                                 lparam->param_type());

            auto declaration = factory_->CreateValDeclaration(
                    rparam->param_name()->ToString(), false,
                    rparam->param_type(), nullptr, scope, true,
                    node->position());
            scope->Declare(declaration->name(), declaration);

            rproto->add_paramter(rparam);
        }
        return true;
    }

    if (lproto->paramter_size() != rproto->paramter_size()) {
        ThrowError(node, "target type can not accept rval, %s vs %s",
                   lproto->Type::ToString().c_str(),
                   rproto->Type::ToString().c_str());
        return false;
    }

    for (int i = 0; i < lproto->paramter_size(); ++i) {
        auto lparam = lproto->paramter(i);
        auto rparam = rproto->paramter(i);

        if (lparam->param_type()->IsUnknown()) {
            ThrowError(node, "target type has unknown type");
            return false;
        }

        if (rparam->param_type()->IsUnknown()) {
            rproto->paramter(i)->set_param_type(lparam->param_type());
            auto param_val = DCHECK_NOTNULL(
                    scope->FindOrNullLocal(rparam->param_name()));
            DCHECK_NOTNULL(param_val->declaration()->AsValDeclaration())
                    ->set_type(lparam->param_type());
        } else {
            if (!lparam->param_type()->CanAcceptFrom(rparam->param_type())) {
                ThrowError(node, "target type can not accept rval, %s vs %s",
                           lproto->Type::ToString().c_str(),
                           rproto->Type::ToString().c_str());
                return false;
            }
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//// class Checker
////////////////////////////////////////////////////////////////////////////////
Checker::Checker(TypeFactory *types, ParsedUnitMap *all_units, Scope *global,
                 Zone *zone)
    : types_(DCHECK_NOTNULL(types))
    , all_units_(DCHECK_NOTNULL(all_units))
    , all_modules_(new (DCHECK_NOTNULL(zone)) ParsedModuleMap(zone))
    , global_(DCHECK_NOTNULL(global))
    , zone_(DCHECK_NOTNULL(zone)) {
    DCHECK_EQ(GLOBAL_SCOPE, global_->type());
}

// global
//   - module
//       - unit
//           - function
//               - block
bool Checker::Run() {
    if (!CheckPackageImporter()) {
        return false;
    }

    auto found = all_modules_->Get(kMainValue);
    if (!found) {
        ThrowError(nullptr, nullptr, "`main' module not found!");
        return false;
    }

    bool ok = true;
    CheckModule(kMainValue, found->value(), &ok);
    return ok;
}

bool Checker::CheckPackageImporter() {
    ParsedUnitMap::Iterator iter(all_units_);

    // key   : unit name
    // value : statements of unit
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        auto stmts = iter->value();

        if (DCHECK_NOTNULL(stmts)->is_empty()) {
            continue; // ignore empty units.
        }

        if (!stmts->At(0)->IsPackageImporter()) {
            ThrowError(iter->key(), stmts->At(0),
                       "package ... with ... statement not found.");
            return false;
        }

        auto pkg = stmts->At(0)->AsPackageImporter();
        // TODO:

        auto module = DCHECK_NOTNULL(GetOrInsertModule(pkg->package_name()));

        auto has_insert = module->Put(iter->key(), iter->value());
        DCHECK(has_insert);
    }

    return true;
}

RawStringRef Checker::CheckImportList(RawStringRef module_name,
                                      RawStringRef unit_name,
                                      bool *ok) {
    auto found = check_state_.find(module_name->ToString());
    if (found->second == MODULE_CHECKING) {
        *ok = false;
        ThrowError(unit_name, nullptr, "recursive import module: %s",
                   module_name->c_str());
        return nullptr;
    }
    return module_name;
}

ParsedUnitMap *Checker::CheckModule(RawStringRef name,
                                      ParsedUnitMap *all_units,
                                      bool *ok) {
    auto scope = DCHECK_NOTNULL(global_->FindInnerScopeOrNull(name));
    DCHECK_EQ(MODULE_SCOPE, scope->type());
    scope->MergeInnerScopes();

    ParsedUnitMap::Iterator iter(all_units);
    // key   : unit name
    // value : statements of unit
    check_state_.emplace(name->ToString(), MODULE_CHECKING);
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        auto stmts = iter->value();

        DCHECK(stmts->is_not_empty());
        DCHECK(stmts->At(0)->IsPackageImporter());

        auto pkg_stmt = stmts->At(0)->AsPackageImporter();
        PackageImporter::ImportList::Iterator jter(pkg_stmt->mutable_import_list());
        for (jter.Init(); jter.HasNext(); jter.MoveNext()) {
            auto pair = all_modules_->Get(jter->key());

            CheckImportList(jter->key(), iter->key(), ok);
            if (!ok) {
                return nullptr;
            }
            CheckModule(jter->key(), DCHECK_NOTNULL(pair->value()), ok);
            if (!ok) {
                return nullptr;
            }
        }
        CheckUnit(iter->key(), pkg_stmt, scope, iter->value(), ok);
        if (!ok) {
            return nullptr;
        }
    }
    check_state_.emplace(name->ToString(), MODULE_CHECKED);
    return all_units;
}

int Checker::CheckUnit(RawStringRef name,
                       PackageImporter *pkg_metadata,
                       Scope *module_scope,
                       ZoneVector<Statement *> *stmts,
                       bool *ok) {
    DCHECK_EQ(MODULE_SCOPE, module_scope->type());

    CheckingAstVisitor visitor(types_,
                               name,
                               pkg_metadata->mutable_import_list(),
                               module_scope->outter_scope(),
                               module_scope,
                               this,
                               zone_);
    for (int i = 0; i < stmts->size(); ++i) {
        stmts->At(i)->Accept(&visitor);
        if (has_error()) {
            *ok = false;
            break;
        }

        if (visitor.has_analysis_expression()) {
            stmts->Set(i, visitor.AnalysisExpression());
            visitor.PopAnalysisExpression();

        }
        visitor.PopEvalType();
    }
    return 0;
}

ParsedUnitMap *Checker::GetOrInsertModule(RawStringRef name) {
    bool has_insert = false;

    auto pair = all_modules_->GetOrInsert(name, &has_insert);
    if (has_insert) {
        check_state_.emplace(name->ToString(), MODULE_READY);
        pair->set_value(new (zone_) ParsedUnitMap(zone_));
    }
    return pair->value();
}

void Checker::ThrowError(RawStringRef unit_name, AstNode *node,
                         const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    VThrowError(unit_name, node, fmt, ap);
    va_end(ap);
}

void Checker::VThrowError(RawStringRef unit_name, AstNode *node,
                          const char *fmt, va_list ap) {
    has_error_ = true;
    last_error_.column    = 0;
    last_error_.line      = 0;
    last_error_.position  = node ? node->position() : 0;
    last_error_.file_name = unit_name ? unit_name->ToString() : "";

    va_list copied;
    int len = 512, rv = len;
    std::string buf;
    do {
        len = rv + 512;
        buf.resize(len, 0);
        va_copy(copied, ap);
        rv = vsnprintf(&buf[0], len, fmt, ap);
        va_copy(ap, copied);
    } while (rv > len);
    buf.resize(strlen(buf.c_str()), 0);
    last_error_.message = std::move(buf);
}

} // namespace mio
