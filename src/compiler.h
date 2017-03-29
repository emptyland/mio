#ifndef MIO_COMPILER_H
#define MIO_COMPILER_H

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "zone.h"
#include "code-label.h"
#include <unordered_map>

namespace mio {

class Statement;
class TypeFactory;
class Scope;
class SimpleFileSystem;
class FunctionEntry;

// [unitName, [statement]]
typedef ZoneHashMap<RawStringRef, ZoneVector<Statement *> *> CompiledUnitMap;

// [moduleName [unitName, statements]]
typedef ZoneHashMap<RawStringRef, CompiledUnitMap *> CompiledModuleMap;

// [pc, <name, entry>]
typedef std::unordered_map<int, std::tuple<std::string, FunctionEntry*>>
    FunctionInfoMap;

struct ParsingError {
    int column;
    int line;
    int position;
    std::string file_name;
    std::string message;

    ParsingError();

    static ParsingError NoError();

    std::string ToString() const;
}; // struct ParsingError

class FunctionEntry {
public:
    FunctionEntry() : offset_(0), is_native_(false) {}

    DEF_PROP_RW(int, offset)
    DEF_PROP_RW(bool, is_native)
    DEF_MUTABLE_GETTER(CodeLabel, label)

    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionEntry)
private:
    int offset_;
    CodeLabel label_;
    bool is_native_;
}; // class FunctionEntry

class Compiler {
public:
    static CompiledUnitMap *ParseProject(const char *project_dir,
                                         SimpleFileSystem *sfs,
                                         TypeFactory *types,
                                         Scope *global,
                                         Zone *zone,
                                         ParsingError *error);

    Compiler() = delete;
    ~Compiler() = delete;
    DISALLOW_IMPLICIT_CONSTRUCTORS(Compiler)
};

} // namespace mio

#endif // MIO_COMPILER_H
