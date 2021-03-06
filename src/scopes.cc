#include "scopes.h"
#include "ast.h"
#include "text-output-stream.h"

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

Scope *Scope::FindOuterScopeOrNull(ScopeType type) {
    auto scope = this;
    while (scope) {
        if (scope->type() == type) {
            break;
        }
        scope = scope->outter_scope_;
    }
    return scope;
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
    auto var = new (zone_) Variable(declaration, zone_->generated_id(),
                                    declaration->position());
    pair->set_value(var);
    declaration->set_instance(var);
    return var;
}

Variable *Scope::Declare(RawStringRef name, Variable *link, int position) {
    bool has_insert = false;
    auto pair = declarations_.GetOrInsert(name, &has_insert);
    if (!has_insert) {
        return nullptr;
    }
    auto var = new (zone_) Variable(link, this, zone_->generated_id(), position);
    pair->set_value(var);
    return var;
}

std::string Scope::MakeFullName(RawStringRef name) {
    std::string full_name;
    int i = 0;
    auto scope = this;
    while (scope->type() != GLOBAL_SCOPE) {
        full_name = "::" + full_name;
        if (scope->name() == RawString::kEmpty) {
            full_name = TextOutputStream::sprintf("<%d>", i) + full_name;
        } else {
            full_name = scope->name()->ToString() + full_name;
        }
        scope = scope->outter_scope_;
        ++i;
    }
    full_name = "::" + full_name;
    full_name.append(name->ToString());
    return full_name;
}

bool Scope::MergeInnerScopes(MergingConflicts *conflicts) {
    ZoneVector<Scope *> new_inner_scopes(zone_);

    conflicts->clear();
    for (int i = 0; i < inner_scopes_.size(); ++i) {
        auto inner = inner_scopes_.At(i);

        DeclaratedMap::Iterator iter(&inner->declarations_);
        for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
            auto var = iter->value();

            bool has_insert = false;
            auto pair = declarations_.GetOrInsert(iter->key(), &has_insert);
            if (!has_insert) {
                auto vars = &(*conflicts)[iter->key()->ToString()];
                if (vars->empty()) {
                    vars->push_back(pair->value());
                }
                vars->push_back(var);
            }
            pair->set_value(var);
        }

        for (int j = 0; j < inner->inner_scopes_.size(); ++j) {
            inner->inner_scopes_.At(j)->outter_scope_ = this;
            new_inner_scopes.Add(inner->inner_scopes_.At(j));
        }
    }
    if (conflicts->empty()) {
        // adjust variables' scope
        DeclaratedMap::Iterator iter(&declarations_);
        for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
            if (iter->value()->scope() != this) {
                iter->value()->declaration()->set_scope(this);
            }
        }

        inner_scopes_.Clear();
        inner_scopes_.Assign(std::move(new_inner_scopes));
    }
    return conflicts->empty();
}

/*static*/ void Scope::TEST_PrintAllVariables(int level, Scope *scope) {
    std::string indent(level, '-');

    printf("%s=====%s: %d=====\n", indent.c_str(), scope->name_->c_str(),
           scope->type_);

    DeclaratedMap::Iterator iter(&scope->declarations_);
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        printf("%s%s\n", indent.c_str(), iter->key()->c_str());
    }
    for (int i = 0; i < scope->inner_scopes_.size(); ++i) {
        TEST_PrintAllVariables(level + 1, scope->inner_scopes_.At(i));
    }
}

} // namespace mio
