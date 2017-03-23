#ifndef MIO_CHECKER_H_
#define MIO_CHECKER_H_

#include "compiler.h"
#include "base.h"
#include <map>

namespace mio {

class CheckingAstVisitor;

class AstNode;
class PackageImporter;
class TypeFactory;
class Scope;
class Zone;

class Checker {
public:
    enum CheckState: int {
        MODULE_READY,
        MODULE_CHECKING,
        MODULE_CHECKED,
    };

    Checker(TypeFactory *types,
            CompiledUnitMap *all_units, Scope *global, Zone *zone);

    DEF_GETTER(bool, has_error)
    DEF_GETTER(ParsingError, last_error)

    bool Run();
    bool CheckPackageImporter();

    CompiledUnitMap *all_units() const { return all_units_; }
    CompiledModuleMap *mutable_all_modules() { return &all_modules_; }

    RawStringRef CheckImportList(RawStringRef module_name,
                                 RawStringRef unit_name,
                                 bool *ok);
    CompiledUnitMap *CheckModule(RawStringRef name, CompiledUnitMap *all_units,
                                 bool *ok);
    int CheckUnit(RawStringRef name,
                  PackageImporter *pkg_metadata,
                  Scope *module_scope,
                  ZoneVector<Statement *> *stmts,
                  bool *ok);
    

    friend class CheckingAstVisitor;

    DISALLOW_IMPLICIT_CONSTRUCTORS(Checker);
private:
    CompiledUnitMap *GetOrInsertModule(RawStringRef name);

    void ThrowError(RawStringRef file_name, AstNode *node, const char *fmt, ...);
    void VThrowError(RawStringRef file_name, AstNode *node, const char *fmt,
                     va_list ap);

    // [moduleName, checkingState]
    std::map<std::string, CheckState> check_state_;
    TypeFactory *types_;
    CompiledUnitMap *all_units_;
    CompiledModuleMap all_modules_;
    Scope *global_;
    Zone *zone_;
    bool has_error_ = false;
    ParsingError last_error_;
};

} // namespace mio


#endif // MIO_CHECKER_H_
