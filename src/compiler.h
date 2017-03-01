#ifndef MIO_COMPILER_H
#define MIO_COMPILER_H

#include "zone-hash-map.h"
#include "zone-vector.h"
#include "zone.h"

namespace mio {

class Statement;

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
}; // struct ParsingError

} // namespace mio

#endif // MIO_COMPILER_H
