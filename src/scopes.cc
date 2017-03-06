#include "scopes.h"
#include "ast.h"

namespace mio {

Scope::Scope(Scope *outter_scope, ScopeType type, Zone *zone)
    : outter_scope_(outter_scope)
    , inner_scopes_(DCHECK_NOTNULL(zone))
    , declarations_(DCHECK_NOTNULL(zone))
    , type_(type)
    , zone_(DCHECK_NOTNULL(zone)) {

    if (outter_scope_) {
        for (int i = 0; i < outter_scope_->inner_scopes_.size(); ++i) {
            if (this == outter_scope_->inner_scopes_.At(i)) {
                return;
            }
        }
        outter_scope_->inner_scopes_.Add(this);
    }
}

Scope *Scope::FindInnerScopeOrNull(RawStringRef name) const {
    for (int i = 0; i < inner_scopes_.size(); ++i) {
        if (inner_scopes_.At(i)->name()->Compare(name) == 0) {
            return inner_scopes_.At(i);
        }
    }
    return nullptr;
}

Variable *Scope::FindOrNullLocal(RawStringRef name) {
    auto pair = declarations_.Get(name);
    return pair ? pair->value() : nullptr;
}

Variable *Scope::FindOrNullRecursive(RawStringRef name, Scope **owned) {
    auto var = FindOrNullLocal(name);
    if (var) {
        *owned = this;
        return var;
    }

    auto up_layout = outter_scope_;
    while (up_layout) {
        var = up_layout->FindOrNullLocal(name);
        if (var) {
            *owned = up_layout;
            break;
        }
        up_layout = up_layout->outter_scope_;
    }
    return var;
}

Variable *Scope::FindOrNullDownTo(RawStringRef name, Scope **owned) {
    auto found = FindOrNullLocal(name);
    if (found) {
        *owned = this;
        return found;
    }

    for (int i = 0; i < inner_scopes_.size(); ++i) {
        auto scope = inner_scopes_.At(i);

        found = scope->FindOrNullDownTo(name, owned);
        if (found) {
            *owned = scope;
            return found;
        }
    }

    return nullptr;
}

Variable *Scope::Declare(RawStringRef name, Declaration *declaration) {
    bool has_insert = false;
    auto pair = declarations_.GetOrInsert(name, &has_insert);
    if (!has_insert) {
        return nullptr;
    }
    auto var = new (zone_) Variable(declaration, declaration->position());
    pair->set_value(var);
    return var;
}

void Scope::MergeInnerScopes() {
    ZoneVector<Scope *> new_inner_scopes(zone_);

    for (int i = 0; i < inner_scopes_.size(); ++i) {
        auto inner = inner_scopes_.At(i);

        DeclaratedMap::Iterator iter(&inner->declarations_);
        for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
            auto var = iter->value();

            var->declaration()->set_scope(this);
            declarations_.Put(iter->key(), var);
        }

        for (int j = 0; j < inner->inner_scopes_.size(); ++j) {
            inner->inner_scopes_.At(j)->outter_scope_ = this;
            new_inner_scopes.Add(inner->inner_scopes_.At(j));
        }
    }
    inner_scopes_.Clear();
    inner_scopes_.Assign(std::move(new_inner_scopes));
}

} // namespace mio
