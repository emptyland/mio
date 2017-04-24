#include "checker.h"
#include "scopes.h"
#include "types.h"
#include "ast.h"
#include "text-output-stream.h"
#include <stack>
#include <vector>

namespace mio {

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

    Scope *fn_scope() const { return fn_scope_; }

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
}; // class FunctionInfoScope

#define ACCEPT_REPLACE_EXPRESSION(node, field) \
    (node)->field()->Accept(this); \
    if (has_error()) { \
        return; \
    } \
    if (has_analysis_expression()) { \
        auto expr = AnalysisExpression(); \
        if (expr->IsVariable()) { \
            auto var = expr->AsVariable(); \
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
    node->field()->At(idx)->Accept(this); \
    if (has_error()) { \
        return; \
    } \
    if (has_analysis_expression()) { \
        auto expr = AnalysisExpression(); \
        if (expr->IsVariable()) { \
            auto var = expr->AsVariable(); \
            if (var->scope()->type() != MODULE_SCOPE && \
                var->scope()->type() != UNIT_SCOPE && \
                var->declaration()->position() > node->position()) { \
                ThrowError(node, "symbol \'%s\' is not found.", \
                var->declaration()->name()->c_str()); \
                return; \
            } \
        } \
        node->field()->Set(idx, AnalysisExpression()); \
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
    virtual void VisitUnaryOperation(UnaryOperation *node) override;
    virtual void VisitAssignment(Assignment *node) override;
    virtual void VisitBinaryOperation(BinaryOperation *node) override;
    virtual void VisitSymbol(Symbol *node) override;
    virtual void VisitSmiLiteral(SmiLiteral *node) override;
    virtual void VisitFloatLiteral(FloatLiteral *node) override;
    virtual void VisitStringLiteral(StringLiteral *node) override;
    virtual void VisitIfOperation(IfOperation *node) override;
    virtual void VisitBlock(Block *node) override;
    virtual void VisitForeachLoop(ForeachLoop *node) override;
    virtual void VisitReturn(Return *node) override;
    virtual void VisitFunctionDefine(FunctionDefine *node) override;
    virtual void VisitFunctionLiteral(FunctionLiteral *node) override;
    virtual void VisitMapInitializer(MapInitializer *node) override;
    virtual void VisitFieldAccessing(FieldAccessing *node) override;
    virtual void VisitTypeTest(TypeTest *node) override;
    virtual void VisitTypeCast(TypeCast *node) override;

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
        if (node->has_initializer() &&
            !node->type()->CanAcceptFrom(AnalysisType())) {
            ThrowError(node, "val %s can not accept initializer type",
                       node->name()->c_str());
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
        if (node->has_initializer() &&
            !node->type()->CanAcceptFrom(AnalysisType())) {
            ThrowError(node, "var %s can not accept initializer type",
                       node->name()->c_str());
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
    } else {
        ThrowError(node, "this type can not be call.");
    }
}

/*virtual*/ void CheckingAstVisitor::VisitUnaryOperation(UnaryOperation *node) {
    ACCEPT_REPLACE_EXPRESSION(node, operand);
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
            if (!AnalysisType()->IsIntegral()) {
                ThrowError(node, "`not' operator only accept bool type.");
            }
            break;

        default:
            break;
    }
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
            // TODO: string type
            if (!lhs_ty->is_numeric()) {
                ThrowError(node, "operator: `%s' only accept numeric type.",
                           GetOperatorText(node->op()));
            }
            PushEvalType(lhs_ty);
            break;

        case OP_OR:
        case OP_AND:
            if (lhs_ty->GenerateId() != rhs_ty->GenerateId()) {
                ThrowError(node, "operator: `%s' has different type of operands.",
                           GetOperatorText(node->op()));
            }
            if (!lhs_ty->IsIntegral()) {
                ThrowError(node, "operator: `%s' only accept integral type.",
                           GetOperatorText(node->op()));
            }
            PushEvalType(lhs_ty);
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
        if (!var->is_function()) {
            ThrowError(node, "symbol \'%s\', its' type unknown.",
                       node->name()->c_str());
            return;
        }
    }

    PushAnalysisExpression(var);
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
    if (node->mutable_body()->is_empty()) {
        PushEvalType(types_->GetVoid());
        return;
    }

    ScopeHolder holder(node->scope(), &scope_);
    for (int i = 0; i < node->mutable_body()->size(); ++i) {
        ACCEPT_REPLACE_EXPRESSION_I(node, mutable_body, i);
        if (i < node->mutable_body()->size() - 1) {
            PopEvalType();
        }
    }
    // keep the last expression type for assignment type.
}

/*virtual*/ void CheckingAstVisitor::VisitForeachLoop(ForeachLoop *node) {
    ACCEPT_REPLACE_EXPRESSION(node, container);
    auto container_type = AnalysisType();
    if (!container_type->IsMap()) {
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
    } else {
        // TODO:
    }
    node->set_container_type(container_type);
    ACCEPT_REPLACE_EXPRESSION(node, body);
}

/*virtual*/ void CheckingAstVisitor::VisitReturn(Return *node) {
    auto func_scope = DCHECK_NOTNULL(scope_->FindOuterScopeOrNull(FUNCTION_SCOPE));
    if (func_scope->function()->function_literal()->is_assignment()) {
        ThrowError(node, "assignment function don not need return.");
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

/*virtual*/
void CheckingAstVisitor::VisitMapInitializer(MapInitializer *node) {
    auto map_type = node->map_type();
    if (map_type->key()->IsUnknown() || map_type->value()->IsUnknown()) {
        if (node->mutable_pairs()->is_empty()) {
            ThrowError(node, "map initializer has unknwon key and value's type");
            return;
        }
    } else if (map_type->key()->CanNotBeKey()) {
        ThrowError(node, "type %s can not be map key.",
                   map_type->key()->ToString().c_str());
        return;
    }

    auto value_types = new (types_->zone()) ZoneHashMap<int64_t, Type *>(types_->zone());
    for (int i = 0; i < node->mutable_pairs()->size(); ++i) {
        auto pair = node->mutable_pairs()->At(i);
        ACCEPT_REPLACE_EXPRESSION(pair, key);
        auto key = AnalysisType();
        PopEvalType();

        if (map_type->key()->IsUnknown()) {
            map_type->set_key(key);
        } else if (!map_type->key()->CanAcceptFrom(key)) {
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

void CheckingAstVisitor::CheckFunctionCall(FunctionPrototype *proto, Call *node) {
    if (proto->mutable_paramters()->size() !=
        node->mutable_arguments()->size()) {
        ThrowError(node, "call argument number is not be accept (%d vs %d).",
                   node->mutable_arguments()->size(),
                   proto->mutable_paramters()->size());
        return;
    }
    for (int i = 0; i < node->mutable_arguments()->size(); ++i) {
        auto arg   = node->mutable_arguments()->At(i);
        auto param = proto->mutable_paramters()->At(i);

        if (arg->IsFunctionLiteral() &&
            !AcceptOrReduceFunctionLiteral(node, param->param_type(),
                                           arg->AsFunctionLiteral())) {
                return;
            }

        ACCEPT_REPLACE_EXPRESSION_I(node, mutable_arguments, i);
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
    }
    PushEvalType(proto->return_type());
}

void CheckingAstVisitor::CheckMapAccessor(Map *map, Call *node) {
    if (node->mutable_arguments()->size() != 1) {
        ThrowError(node, "bad map access calling.");
        return;
    }

    if (node->mutable_arguments()->size() == 1) {
        // Getting
        ACCEPT_REPLACE_EXPRESSION_I(node, mutable_arguments, 0);
        auto key = AnalysisType();
        PopEvalType();

        if (!map->key()->CanAcceptFrom(key)) {
            ThrowError(node->mutable_arguments()->At(0),
                       "map key can not accept input type, (%s vs %s)",
                       map->key()->ToString().c_str(),
                       key->ToString().c_str());
            return;
        }

        Type *types[] = {map->value(), types_->GetVoid()};
        PushEvalType(types_->MergeToFlatUnion(types, arraysize(types)));
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
    if (rproto->mutable_paramters()->is_empty()) {
        for (int i = 0; i < lproto->mutable_paramters()->size(); ++i) {
            auto lparam = lproto->mutable_paramters()->At(i);
            auto rparam = types_->CreateParamter(TextOutputStream::sprintf("_%d", i + 1),
                                                 lparam->param_type());

            auto declaration = factory_->CreateValDeclaration(
                    rparam->param_name()->ToString(), false,
                    rparam->param_type(), nullptr, scope, true,
                    node->position());
            scope->Declare(declaration->name(), declaration);

            rproto->mutable_paramters()->Add(rparam);
        }
        return true;
    }

    if (lproto->mutable_paramters()->size() !=
        rproto->mutable_paramters()->size()) {
        ThrowError(node, "target type can not accept rval, %s vs %s",
                   lproto->Type::ToString().c_str(),
                   rproto->Type::ToString().c_str());
        return false;
    }

    for (int i = 0; i < lproto->mutable_paramters()->size(); ++i) {
        auto lparam = lproto->mutable_paramters()->At(i);
        auto rparam = rproto->mutable_paramters()->At(i);

        if (lparam->param_type()->IsUnknown()) {
            ThrowError(node, "target type has unknown type");
            return false;
        }

        if (rparam->param_type()->IsUnknown()) {
            rproto->mutable_paramters()->At(i)->set_param_type(lparam->param_type());
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
