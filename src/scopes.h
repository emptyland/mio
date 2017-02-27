#ifndef MIO_SCOPES_H_
#define MIO_SCOPES_H_

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "raw-string.h"
#include "zone.h"

namespace mio {

class Declaration;

enum ScopeType: int {
    GLOBAL_SCOPE,
    MODULE_SCOPE,
    FUNCTION_SCOPE,
    BLOCK_SCOPE,
};

class Scope : public ManagedObject {
public:
    Scope(Scope *outter_scope, ScopeType type, Zone *zone);

    Scope *FindInnerScopeOrNull(RawStringRef name) const;
    Declaration *FindOrNullLocal(RawStringRef name);

    Declaration *FindOrNullDownTo(const char *name, Scope **owned) {
        return FindOrNullDownTo(RawString::Create(name, zone_), owned);
    }
    Declaration *FindOrNullDownTo(RawStringRef name, Scope **owned);

    bool Declare(RawStringRef name, Declaration *declaration);

    RawStringRef name() const { return name_; }
    void set_name(RawStringRef name) { name_ = DCHECK_NOTNULL(name); }

    Scope *outter_scope() const { return outter_scope_; }

    bool is_global_scope() const { return type_ == GLOBAL_SCOPE; }
    bool is_module_scope() const { return type_ == MODULE_SCOPE; }
    bool is_function_scope() const { return type_ == FUNCTION_SCOPE; }
    bool is_block_scope() const { return type_ == BLOCK_SCOPE; }

    DISALLOW_IMPLICIT_CONSTRUCTORS(Scope)
private:

    RawStringRef name_ = RawString::kEmpty;
    ScopeType type_;
    Scope *outter_scope_;
    ZoneVector<Scope *> inner_scopes_;
    ZoneHashMap<RawStringRef, Declaration *> declarations_;
    Zone *zone_;
};

} // namespace mio

#endif // MIO_SCOPES_H_
