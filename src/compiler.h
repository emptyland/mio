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
class MemorySegment;
class ObjectFactory;
class FunctionRegister;

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
    int type_id_base;
    int type_id_bytes;
    int constatns_segment_bytes;
    int global_primitive_segment_bytes;
    int global_object_segment_bytes;
};

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
    static ParsedUnitMap *ParseProject(const char *project_dir,
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
                                 MemorySegment *constants,
                                 MemorySegment *p_global,
                                 MemorySegment *o_global,
                                 TypeFactory *types,
                                 ObjectFactory *object_factory,
                                 FunctionRegister *function_register,
                                 CompiledInfo *info);

    Compiler() = delete;
    ~Compiler() = delete;
    DISALLOW_IMPLICIT_CONSTRUCTORS(Compiler)
};

} // namespace mio

#endif // MIO_COMPILER_H
