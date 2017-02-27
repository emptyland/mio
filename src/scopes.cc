#include "scopes.h"

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

} // namespace mio
