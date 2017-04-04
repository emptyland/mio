#include "vm.h"
#include "vm-memory-segment.h"
#include "vm-thread.h"
#include "vm-objects.h"
#include "vm-bitcode-disassembler.h"
#include "zone.h"
#include "simple-file-system.h"
#include "types.h"
#include "scopes.h"
#include "memory-output-stream.h"
#include "malloced-object-factory.h"
#include "simple-function-register.h"

namespace mio {

VM::VM()
    : main_thread_(new Thread(this))
    , constants_(new MemorySegment())
    , p_global_(new MemorySegment())
    , o_global_(new MemorySegment())
    , ast_zone_(new Zone()) {
}

VM::~VM() {
    delete p_global_;
    delete o_global_;
    delete constants_;
    delete main_thread_;
}

//HeapMemoryManagementUnit

bool VM::Init() {
    object_factory_ = new MallocedObjectFactory;
    function_register_ = new SimpleFunctionRegister(o_global_);
    // TODO:
    return true;
}

bool VM::CompileProject(const char *project_dir, ParsingError *error) {
    std::unique_ptr<SimpleFileSystem> sfs(CreatePlatformSimpleFileSystem());
    std::unique_ptr<TypeFactory> types(new TypeFactory(ast_zone_));

    auto scope = new (ast_zone_) Scope(nullptr, GLOBAL_SCOPE, ast_zone_);
    auto all_units = Compiler::ParseProject(project_dir, sfs.get(), types.get(),
                                            scope, ast_zone_, error);
    if (!all_units) {
        return false;
    }

    all_modules_ = Compiler::Check(all_units, types.get(), scope, ast_zone_,
                                   error);
    if (!all_modules_) {
        return false;
    }

    CompiledInfo info;
    Compiler::AstEmitToBitCode(all_modules_, constants_, p_global_, o_global_,
                               types.get(), object_factory_, function_register_,
                               &info);
    DLOG(INFO) << "cs: " << info.constatns_segment_bytes << "\n"
               << "pg: " << info.global_primitive_segment_bytes << "\n"
               << "og: " << info.global_object_segment_bytes;
    return true;
}

int VM::Run() {
    auto entry = DCHECK_NOTNULL(function_register_)->FindOrNull("::main::bootstrap");
    if (!entry) {
        LOG(ERROR) << "main function not found!";
        return -1;
    }

    auto main_ob = make_local(o_global_->Get<HeapObject *>(entry->offset()));
    auto main_fn = main_ob->AsNormalFunction();
    if (!main_fn) {
        LOG(ERROR) << "::main::main symbol is not function!";
        return -1;
    }

    bool ok = true;
    main_thread_->Execute(main_fn, &ok);
    if (!ok) {
        return main_thread_->exit_code();
    }
    return 0;
}

void VM::DisassembleAll(TextOutputStream *stream) {
    std::vector<Local<MIONormalFunction>> all_functions;
    function_register_->GetAllFunctions(&all_functions);

    BitCodeDisassembler dasm(stream);
    for (auto fn : all_functions) {
        dasm.Run(fn);
    }
}

void VM::DisassembleAll(std::string *buf) {
    MemoryOutputStream stream(buf);
    DisassembleAll(&stream);
}

} // namespace mio
