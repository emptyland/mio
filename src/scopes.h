#ifndef MIO_SCOPES_H_
#define MIO_SCOPES_H_

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "raw-string.h"
#include "zone.h"
#include <map>
#include <vector>

namespace mio {

class Declaration;
class Variable;
class FunctionDefine;

enum ScopeType: int {
    GLOBAL_SCOPE,
    MODULE_SCOPE,
    UNIT_SCOPE,
    FUNCTION_SCOPE,
    BLOCK_SCOPE,
};

class Scope : public ManagedObject {
public:
    typedef ZoneHashMap<RawStringRef, Variable *> DeclaratedMap;

    Scope(Scope *outter_scope, ScopeType type, Zone *zone);

    Scope *FindInnerScopeOrNull(const char *name) const {
        return FindInnerScopeOrNull(RawString::Create(name, zone_));
    }
    Scope *FindInnerScopeOrNull(RawStringRef name) const;

    Variable *FindOrNullLocal(const char *name) {
        return FindOrNullLocal(RawString::Create(name, zone_));
    }
    Variable *FindOrNullLocal(RawStringRef name);
    Variable *FindOrNullRecursive(RawStringRef name, Scope **owned);
    Variable *FindOrNullDownTo(const char *name, Scope **owned) {
        return FindOrNullDownTo(RawString::Create(name, zone_), owned);
    }
    Variable *FindOrNullDownTo(RawStringRef name, Scope **owned);

    Variable *Declare(RawStringRef name, Declaration *declaration);


    typedef std::map<std::string, std::vector<Variable *>> MergingConflicts;
    // merge the low 1 layout scopes, in this layout.
    // 
    bool MergeInnerScopes() {
        MergingConflicts conflicts;
        return MergeInnerScopes(&conflicts);
    }
    bool MergeInnerScopes(MergingConflicts *conflicts);

    RawStringRef name() const { return name_; }
    void set_name(RawStringRef name) { name_ = DCHECK_NOTNULL(name); }

    FunctionDefine *function() const { return function_; }
    void set_function(FunctionDefine *function) { function_ = function; }

    Scope *outter_scope() const { return outter_scope_; }

    ScopeType type() const { return type_; }

    bool is_global_scope() const { return type_ == GLOBAL_SCOPE; }
    bool is_module_scope() const { return type_ == MODULE_SCOPE; }
    bool is_function_scope() const { return type_ == FUNCTION_SCOPE; }
    bool is_block_scope() const { return type_ == BLOCK_SCOPE; }

    static void TEST_PrintAllVariables(int level, Scope *scope);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Scope)
private:

    RawStringRef name_ = RawString::kEmpty;
    ScopeType type_;
    Scope *outter_scope_;
    ZoneVector<Scope *> inner_scopes_;
    DeclaratedMap declarations_;
    FunctionDefine *function_ = nullptr;
    Zone *zone_;
};

} // namespace mio

#endif // MIO_SCOPES_H_
