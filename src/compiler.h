#ifndef MIO_COMPILER_H
#define MIO_COMPILER_H

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "zone.h"
#include "handles.h"
#include <unordered_map>

namespace mio {

template<class K, class V> class MIOHashMapStub;
template<class T> class MIOArrayStub;

class Statement;
class TypeFactory;
class Scope;
class SimpleFileSystem;
class FunctionEntry;
class MemorySegment;
class ObjectFactory;
class ObjectExtraFactory;
class FunctionRegister;
class CodeRef;
class CodeCache;
class TraceTree;

class MIOHashMap;
class MIOFunction;
class MIOGeneratedFunction;
class MIOString;
class MIOReflectionType;

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
    int global_primitive_segment_bytes;
    int global_object_segment_bytes;
    int next_function_id;
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
                                 MIOHashMapStub<Handle<MIOString>, mio_i32_t> *all_var,
                                 MIOArrayStub<Handle<MIOReflectionType>> *all_type,
                                 std::unordered_map<int64_t, int> *type_id2index,
                                 CompiledInfo *info,
                                 int next_function_id);

    static void BitCodeToNativeCodeFragment(MIOFunction *fn,
                                            int pc,
                                            int id,
                                            TraceTree *tree,
                                            CodeCache *cc,
                                            CodeRef *cr);

    static void BitCodeToNativeCode(MIOFunction *fn,
                                    int pc,
                                    int id,
                                    TraceTree *tree,
                                    CodeCache *cc,
                                    CodeRef *cr);

    Compiler() = delete;
    ~Compiler() = delete;
    DISALLOW_IMPLICIT_CONSTRUCTORS(Compiler)
};

} // namespace mio

#endif // MIO_COMPILER_H
