#ifndef MIO_BITCODE_EMITTER_H_
#define MIO_BITCODE_EMITTER_H_

#include "compiler.h"
#include "vm-objects.h"
#include "vm-object-surface.h"
#include "handles.h"
#include "vm-bitcode.h"
#include <unordered_set>

namespace mio {

class MemorySegment;
class EmittingAstVisitor;
class BitCodeBuilder;
class TypeFactory;
class ObjectFactory;
class ObjectExtraFactory;
class FunctionRegister;
class Declaration;
class PackageImporter;
class EmittedScope;

class BitCodeEmitter {
public:
    BitCodeEmitter(MemorySegment *p_global,
                   MemorySegment *o_global,
                   TypeFactory *types,
                   ObjectFactory *object_factory,
                   ObjectExtraFactory *extra_factory,
                   FunctionRegister *function_register,
                   MIOHashMapStub<Handle<MIOString>, mio_i32_t> *all_var,
                   MIOArrayStub<Handle<MIOReflectionType>> *all_type,
                   std::unordered_map<int64_t, int> *type_id2index,
                   int next_function_id);
    ~BitCodeEmitter();

    void Init();

    bool Run(RawStringRef module_name, RawStringRef unit_name,
             ZoneVector<Statement *> *stmts);

    bool Run(ParsedModuleMap *all_modules, CompiledInfo *info);

    friend class EmittingAstVisitor;
    DISALLOW_IMPLICIT_CONSTRUCTORS(BitCodeEmitter);

private:
    bool EmitModule(RawStringRef module_name,
                    ParsedUnitMap *all_units,
                    ParsedModuleMap *all_modules);

    bool ProcessImportList(PackageImporter *pkg,
                           EmittedScope *info,
                           ParsedModuleMap *all_modules);

    inline int GenerateFunctionId() { return next_function_id_++; }

    MemorySegment *p_global_;
    MemorySegment *o_global_;
    TypeFactory *types_;
    ObjectFactory *object_factory_;
    ObjectExtraFactory *extra_factory_;
    FunctionRegister *function_register_;
    MIOHashMapStub<Handle<MIOString>, mio_i32_t> *all_var_;
    MIOArrayStub<Handle<MIOReflectionType>> *all_type_;
    std::unordered_map<int64_t, int> *type_id2index_;
    std::unordered_set<Declaration *> emitted_;
    std::unordered_set<std::string> imported_;
    int all_type_base_ = 0;
    int next_function_id_;
};

} // namespace mio

#endif // MIO_BITCODE_EMITTER_H_
