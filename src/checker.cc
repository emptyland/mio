#include "checker.h"
#include "scopes.h"
#include "types.h"
#include "ast.h"
#include <stack>
#include <vector>

namespace mio {

#define CHECK_OK ok); if (!*ok) { return 0; } ((void)0

//namespace {

class CheckingAstVisitor : public DoNothingAstVisitor {
public:
    CheckingAstVisitor(TypeFactory *types,
                       RawStringRef unit_name,
                       PackageImporter::ImportList *import_list,
                       Scope *global,
                       Scope *scope,
                       Checker *checker)
        : types_(DCHECK_NOTNULL(types))
        , unit_name_(DCHECK_NOTNULL(unit_name))
        , import_list_(DCHECK_NOTNULL(import_list))
        , global_(DCHECK_NOTNULL(global))
        , current_scope_(DCHECK_NOTNULL(scope))
        , checker_(DCHECK_NOTNULL(checker)) {}

    virtual void VisitValDeclaration(ValDeclaration *node) override {
        if (node->has_initializer()) {
            node->initializer()->Accept(this);
        }
        if (has_error()) {
            return;
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

    virtual void VisitVarDeclaration(VarDeclaration *node) override {
        if (node->has_initializer()) {
            node->initializer()->Accept(this);
        }
        if (has_error()) {
            return;
        }

        if (node->type() == types_->GetUnknown()) {
            DCHECK_NOTNULL(node->initializer());
            node->set_type(AnalysisType());
        } else {
            if (!node->type()->CanAcceptFrom(AnalysisType())) {
                ThrowError(node, "var %s can not accept initializer type",
                           node->name()->c_str());
            }
        }
        SetEvalType(types_->GetVoid());
    }

    virtual void VisitUnaryOperation(UnaryOperation *node) override {
        node->operand()->Accept(this);
        if (has_analysis_expression()) {
            node->set_operand(AnalysisExpression());
        }
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

    virtual void VisitBinaryOperation(BinaryOperation *node) override {
        node->lhs()->Accept(this);
        if (has_analysis_expression()) {
            node->set_lhs(AnalysisExpression());
        }
        auto lhs_ty = AnalysisType();
        PopEvalType();

        node->rhs()->Accept(this);
        if (has_analysis_expression()) {
            node->set_rhs(AnalysisExpression());
        }
        auto rhs_ty = AnalysisType();
        PopEvalType();

        switch(node->op()) {
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_MOD:
                if (lhs_ty->id() != rhs_ty->id()) {
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
                if (lhs_ty->id() != rhs_ty->id()) {
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
                if (lhs_ty->id() != rhs_ty->id()) {
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
    }

    virtual void VisitSymbol(Symbol *node) override {
        Scope *scope = current_scope_;

        if (node->has_name_space()) {
            auto pair = import_list_->Get(node->name_space());
            if (!pair) {
                ThrowError(node, "pacakge: %s has not import yet.",
                           node->name_space()->c_str());
                return;
            }
            scope = global_->FindInnerScopeOrNull(node->name_space());
        }

        Scope *owned;
        auto var = scope->FindOrNullRecursive(node->name(), &owned);
        if (!var) {
            ThrowError(node, "symbol %s not found", node->name()->c_str());
            return;
        }
        DCHECK_EQ(var->scope(), owned);

        if (var->type() == types_->GetUnknown()) {
            if (!var->is_function()) {
                ThrowError(node, "symbol %s not found", node->name()->c_str());
                return;
            }
        }

        PushAnalysisExpression(var);
        PushEvalType(var->type());
    }

    virtual void VisitSmiLiteral(SmiLiteral *node) override {
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

    virtual void VisitBlock(Block *node) override {
        if (node->mutable_body()->is_empty()) {
            PushEvalType(types_->GetVoid());
            return;
        }

        for (int i = 0; i < node->mutable_body()->size() - 1; ++i) {
            node->mutable_body()->At(i)->Accept(this);
            if (has_error()) {
                return;
            }
            PopEvalType();
        }

        // use the last expression type
        node->mutable_body()->last()->Accept(this);
    }

    virtual void VisitFunctionDefine(FunctionDefine *node) override {
        // TODO:
        
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
    Scope *current_scope_;
    Checker *checker_;
    std::stack<Type *> type_stack_;
    std::stack<Expression *> expr_stack_;
};

//} // namespace

Checker::Checker(TypeFactory *types, CompiledUnitMap *all_units, Scope *global,
                 Zone *zone)
    : types_(DCHECK_NOTNULL(types))
    , all_units_(DCHECK_NOTNULL(all_units))
    , all_modules_(DCHECK_NOTNULL(zone))
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

    auto found = all_modules_.Get(kMainValue);
    if (!found) {
        ThrowError(nullptr, nullptr, "`main' module not found!");
        return false;
    }

    bool ok = true;
    CheckModule(kMainValue, found->value(), &ok);
    return ok;
}

bool Checker::CheckPackageImporter() {
    CompiledUnitMap::Iterator iter(all_units_);

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

CompiledUnitMap *Checker::CheckModule(RawStringRef name,
                                      CompiledUnitMap *all_units,
                                      bool *ok) {
    //DLOG(INFO) << "check module: " << name->ToString();

    auto scope = DCHECK_NOTNULL(global_->FindInnerScopeOrNull(name));
    DCHECK_EQ(MODULE_SCOPE, scope->type());
    scope->MergeInnerScopes();

    CompiledUnitMap::Iterator iter(all_units);
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
            auto pair = all_modules_.Get(jter->key());

            CheckImportList(jter->key(), iter->key(), CHECK_OK);
            CheckModule(jter->key(), DCHECK_NOTNULL(pair->value()), CHECK_OK);
        }
        CheckUnit(iter->key(), pkg_stmt, scope, iter->value(), CHECK_OK);
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
                               this);
    for (int i = 0; i < stmts->size(); ++i) {
        stmts->At(i)->Accept(&visitor);

        if (visitor.has_analysis_expression()) {
            stmts->Set(i, visitor.AnalysisExpression());
            visitor.PopAnalysisExpression();

        }
        visitor.PopEvalType();
    }
    return 0;
}

CompiledUnitMap *Checker::GetOrInsertModule(RawStringRef name) {
    bool has_insert = false;

    auto pair = all_modules_.GetOrInsert(name, &has_insert);
    if (has_insert) {
        check_state_.emplace(name->ToString(), MODULE_READY);
        pair->set_value(new (zone_) CompiledUnitMap(zone_));
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
