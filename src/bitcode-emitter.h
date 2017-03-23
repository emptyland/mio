#ifndef MIO_BITCODE_EMITTER_H_
#define MIO_BITCODE_EMITTER_H_

#include "compiler.h"
#include "vm-bitcode.h"
#include <unordered_map>

namespace mio {

class MemorySegment;
class EmittingAstVisitor;
class BitCodeBuilder;
class TypeFactory;

struct VMValue {
    BCSegment segment;
    int       offset;
    int       size;
};

class BitCodeEmitter {
public:
    BitCodeEmitter(MemorySegment *code,
                   MemorySegment *constants,
                   MemorySegment *p_global,
                   MemorySegment *o_global,
                   TypeFactory *types);
    ~BitCodeEmitter();

    bool Run(RawStringRef module_name, RawStringRef unit_name,
             ZoneVector<Statement *> *stmts);

    friend class EmittingAstVisitor;
    DISALLOW_IMPLICIT_CONSTRUCTORS(BitCodeEmitter);
private:
    BitCodeBuilder *builder_;
    MemorySegment *constants_;
    MemorySegment *p_global_;
    MemorySegment *o_global_;
    TypeFactory *types_;
};

} // namespace mio

#endif // MIO_BITCODE_EMITTER_H_
