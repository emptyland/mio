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

Declaration *Scope::FindOrNullLocal(RawStringRef name) {
    auto pair = declarations_.Get(name);
    return pair ? pair->value() : nullptr;
}

Declaration *Scope::FindOrNullDownTo(RawStringRef name, Scope **owned) {
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

bool Scope::Declare(RawStringRef name, Declaration *declaration) {
    bool has_insert = false;
    auto pair = declarations_.GetOrInsert(name, &has_insert);
    if (!has_insert) {
        return false;
    }
    pair->set_value(declaration);
    return true;
}

void Scope::MergeInnerScopes() {
    ZoneVector<Scope *> new_inner_scopes(zone_);

    for (int i = 0; i < inner_scopes_.size(); ++i) {
        auto inner = inner_scopes_.At(i);

        DeclaratedMap::Iterator iter(&inner->declarations_);
        for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
            auto declaration = iter->value();

            declaration->set_scope(this);
            declarations_.Put(iter->key(), declaration);
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
