#include "checker.h"
#include "scopes.h"
#include "types.h"
#include "ast.h"

namespace mio {

#define CHECK_OK ok); if (!*ok) { return 0; } ((void)0

//namespace {

class CheckingAstVisitor : public DoNothingAstVisitor {
public:
    CheckingAstVisitor();

    virtual void VisitValDeclaration(ValDeclaration *node) override {
        if (node->has_initializer()) {
            node->initializer()->Accept(this);
        }

        if (node->type() == types_->GetUnknown()) {
            DCHECK_NOTNULL(node->initializer());
            node->set_type(AnalysisType());
            PopEvalType();
        } else {
//            if (!node->type()->CanAcceptFrom(AnalysisType())) {
//                ThrowError(node, "val %s can not accept initializer type",
//                           node->name()->c_str());
//                PopEvalType();
//            }
        }
        PushEvalType(types_->GetVoid());
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
            PopEvalType();
        } else {
//            if (!node->type()->CanAcceptFrom(AnalysisType())) {
//                ThrowError(node, "var %s can not accept initializer type",
//                           node->name()->c_str());
//                PopEvalType();
//            }
        }
        PushEvalType(types_->GetVoid());
    }

    virtual void VisitUnaryOperation(UnaryOperation *node) override {
        node->operand()->Accept(this);

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
        // keep type
    }

    virtual void VisitBinaryOperation(BinaryOperation *node) override {
        node->lhs()->Accept(this);
        auto lhs_ty = AnalysisType();
        PopEvalType();

        node->rhs()->Accept(this);
        auto rhs_ty = AnalysisType();
        PopEvalType();

        switch(node->op()) {
            case OP_ADD:
                if (lhs_ty->id() != rhs_ty->id()) {
                    ThrowError(node, "operator: `+' has different type of operands.");
                }
                if (!lhs_ty->is_numeric()) {
                    ThrowError(node, "operator: `+' only accept numeric type.");
                }
                PushEvalType(lhs_ty);
                break;

                // TODO:
            default:
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

        auto declaration = scope->FindOrNullLocal(node->name());
        if (!declaration) {
            ThrowError(node, "symbol %s not found", node->name()->c_str());
            return;
        }

        PushEvalType(declaration->type());
    }


    Type *AnalysisType() { // TODO:
        return types_->GetUnknown();
    };

    bool has_error() { // TODO:
        return false;
    }

private:

    void PushEvalType(Type *type) {
        // TODO:
    }

    void PopEvalType() {
        // TODO:
    }

    void ThrowError(AstNode *node, const char *fmt, ...) {
        // TODO:
    }

    TypeFactory *types_;
    PackageImporter::ImportList *import_list_;
    Scope *global_;
    Scope *current_scope_;
};

//} // namespace

Checker::Checker(CompiledUnitMap *all_units, Scope *global, Zone *zone)
    : all_units_(DCHECK_NOTNULL(all_units))
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
        ThrowError("main module not found!");
        return false;
    }


    return false;
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
            ThrowError("%s:package ... with ... statement not found.",
                       iter->key()->c_str());
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

CompiledUnitMap *Checker::CheckModule(RawStringRef name,
                                      CompiledUnitMap *all_units,
                                      bool *ok) {
    auto scope = global_->FindInnerScopeOrNull(name);
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

    for (int i = 0; i < stmts->size(); ++i) {

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

void Checker::ThrowError(const char *fmt, ...) {
    // TODO:
}

} // namespace mio
