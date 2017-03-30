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
class ObjectFactory;
class FunctionRegister;

class BitCodeEmitter {
public:
    BitCodeEmitter(MemorySegment *constants,
                   MemorySegment *p_global,
                   MemorySegment *o_global,
                   TypeFactory *types,
                   ObjectFactory *object_factory,
                   FunctionRegister *function_register);
    ~BitCodeEmitter();

    bool Run(RawStringRef module_name, RawStringRef unit_name,
             ZoneVector<Statement *> *stmts);

    friend class EmittingAstVisitor;
    DISALLOW_IMPLICIT_CONSTRUCTORS(BitCodeEmitter);

private:
    MemorySegment *constants_;
    MemorySegment *p_global_;
    MemorySegment *o_global_;
    TypeFactory *types_;
    ObjectFactory *object_factory_;
    FunctionRegister *function_register_;
};

} // namespace mio

#endif // MIO_BITCODE_EMITTER_H_
