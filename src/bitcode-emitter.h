#ifndef MIO_BITCODE_EMITTER_H_
#define MIO_BITCODE_EMITTER_H_

#include "compiler.h"
#include "vm-bitcode.h"
#include <unordered_set>

namespace mio {

class MemorySegment;
class EmittingAstVisitor;
class BitCodeBuilder;
class TypeFactory;
class ObjectFactory;
class FunctionRegister;
class Declaration;
class PackageImporter;
    class EmittedScope;

class BitCodeEmitter {
public:
    BitCodeEmitter(MemorySegment *constants,
                   MemorySegment *p_global,
                   MemorySegment *o_global,
                   TypeFactory *types,
                   ObjectFactory *object_factory,
                   FunctionRegister *function_register);
    ~BitCodeEmitter();

    void Init();

    bool Run(RawStringRef module_name, RawStringRef unit_name,
             ZoneVector<Statement *> *stmts);

    bool Run(CompiledModuleMap *all_modules);

    friend class EmittingAstVisitor;
    DISALLOW_IMPLICIT_CONSTRUCTORS(BitCodeEmitter);

private:
    bool EmitModule(RawStringRef module_name,
                    CompiledUnitMap *all_units,
                    CompiledModuleMap *all_modules);

    bool ProcessImportList(PackageImporter *pkg,
                           EmittedScope *info,
                           CompiledModuleMap *all_modules);

    MemorySegment *constants_;
    MemorySegment *p_global_;
    MemorySegment *o_global_;
    TypeFactory *types_;
    ObjectFactory *object_factory_;
    FunctionRegister *function_register_;
    std::unordered_set<Declaration *> emitted_;
    std::unordered_set<std::string> imported_;
    int type_id_base_ = 0;
    std::unordered_map<int64_t, int> type_id2index_;
};

} // namespace mio

#endif // MIO_BITCODE_EMITTER_H_
