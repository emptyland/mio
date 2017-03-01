#ifndef MIO_CHECKER_H_
#define MIO_CHECKER_H_

#include "compiler.h"
#include "base.h"
#include <map>

namespace mio {

class PackageImporter;

class Scope;
class Zone;

// [moduleName [unitName, statements]]
typedef ZoneHashMap<RawStringRef, CompiledUnitMap *> CompiledModuleMap;

class Checker {
public:
    enum CheckState: int {
        MODULE_READY,
        MODULE_CHECKING,
        MODULE_CHECKED,
    };

    Checker(CompiledUnitMap *all_units, Scope *global, Zone *zone);

    bool Run();
    bool CheckPackageImporter();

    CompiledUnitMap *CheckModule(RawStringRef name, CompiledUnitMap *all_units,
                                 bool *ok);
    int CheckUnit(RawStringRef name,
                  PackageImporter *pkg_metadata,
                  Scope *module_scope,
                  ZoneVector<Statement *> *stmts,
                  bool *ok);
    

    DISALLOW_IMPLICIT_CONSTRUCTORS(Checker);
private:
    CompiledUnitMap *GetOrInsertModule(RawStringRef name);

    void ThrowError(const char *fmt, ...);

    // [moduleName, checkingState]
    std::map<std::string, CheckState> check_state_;
    CompiledUnitMap *all_units_;
    CompiledModuleMap all_modules_;
    Scope *global_;
    Zone *zone_;
};

} // namespace mio


#endif // MIO_CHECKER_H_
