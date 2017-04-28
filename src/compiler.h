#ifndef MIO_COMPILER_H
#define MIO_COMPILER_H

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "zone.h"
#include <unordered_map>

namespace mio {

class Statement;
class TypeFactory;
class Scope;
class SimpleFileSystem;
class FunctionEntry;
class MemorySegment;
class ObjectFactory;
class ObjectExtraFactory;
class FunctionRegister;

class MIOFunction;

// [unitName, [statement]]
typedef ZoneHashMap<RawStringRef, ZoneVector<Statement *> *> ParsedUnitMap;

// [moduleName [unitName, statements]]
typedef ZoneHashMap<RawStringRef, ParsedUnitMap *> ParsedModuleMap;

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

struct CompiledInfo {
    int all_type_base;
    int void_type_index;
    int error_type_index;
    int global_primitive_segment_bytes;
    int global_object_segment_bytes;
};

class FunctionEntry {
public:
    enum Kind {
        NORMAL,
        NATIVE,
    };
    FunctionEntry() : offset_(0), kind_(NORMAL) {}

    DEF_PROP_RW(int, offset)
    DEF_PROP_RW(Kind, kind)

    DISALLOW_IMPLICIT_CONSTRUCTORS(FunctionEntry)
private:
    int  offset_;
    Kind kind_;
}; // class FunctionEntry

class Compiler {
public:
    static ParsedUnitMap *ParseProject(const char *project_dir,
                                       SimpleFileSystem *sfs,
                                       TypeFactory *types,
                                       Scope *global,
                                       Zone *zone,
                                       ParsingError *error);

    static ParsedUnitMap *ParseProject(const char *project_dir,
                                       const char *entry_module,
                                       const std::vector<std::string> &builtin_modules,
                                       const std::vector<std::string> &search_paths,
                                       SimpleFileSystem *sfs,
                                       TypeFactory *types,
                                       Scope *global,
                                       Zone *zone,
                                       ParsingError *error);

    static ParsedModuleMap *Check(ParsedUnitMap *all_units,
                                  TypeFactory *types,
                                  Scope *global,
                                  Zone *zone,
                                  ParsingError *error);

    static void AstEmitToBitCode(ParsedModuleMap *all_modules,
                                 MemorySegment *p_global,
                                 MemorySegment *o_global,
                                 TypeFactory *types,
                                 ObjectFactory *object_factory,
                                 ObjectExtraFactory *extra_factory,
                                 FunctionRegister *function_register,
                                 CompiledInfo *info);

    Compiler() = delete;
    ~Compiler() = delete;
    DISALLOW_IMPLICIT_CONSTRUCTORS(Compiler)
};

} // namespace mio

#endif // MIO_COMPILER_H
