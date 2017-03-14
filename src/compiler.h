#ifndef MIO_COMPILER_H
#define MIO_COMPILER_H

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "zone.h"

namespace mio {

class Statement;
class TypeFactory;
class Scope;
class SimpleFileSystem;

// [unitName, [statement]]
typedef ZoneHashMap<RawStringRef, ZoneVector<Statement *> *> CompiledUnitMap;

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
