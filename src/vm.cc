#include "vm.h"
#include "vm-memory-segment.h"
#include "vm-thread.h"
#include "vm-object-extra-factory.h"
#include "vm-objects.h"
#include "vm-bitcode-disassembler.h"
#include "vm-runtime.h"
#include "zone.h"
#include "simple-file-system.h"
#include "types.h"
#include "scopes.h"
#include "memory-output-stream.h"
#include "simple-function-register.h"
#include "do-nothing-garbage-collector.h"
#include "msg-garbage-collector.h"
#include "source-file-position-dict.h"

namespace mio {

VM::VM()
    : gc_name_("msg")
    , main_thread_(new Thread(this))
    , p_global_(new MemorySegment())
    , o_global_(new MemorySegment())
    , ast_zone_(new Zone())
    , allocator_(new ManagedAllocator())
    , source_position_dict_(new SourceFilePositionDict()) {
}

VM::~VM() {
    delete source_position_dict_;
    delete p_global_;
    delete o_global_;
    delete main_thread_;
    delete gc_;
}

bool VM::Init() {
    if (gc_name_.compare("msg") == 0) {
        gc_ = new MSGGarbageCollector(allocator_, o_global_, main_thread_, false);
    } else if (gc_name_.compare("nogc") == 0) {
        gc_ = new DoNothingGarbageCollector(allocator_);
    } else {
        DLOG(ERROR) << "bad gc name: " << gc_name_;
        return false;
    }
    function_register_ = new SimpleFunctionRegister(o_global_);
    // TODO:
    return true;
}

bool VM::CompileProject(const char *project_dir, ParsingError *error) {
    std::unique_ptr<SimpleFileSystem> sfs(CreatePlatformSimpleFileSystem());
    std::unique_ptr<TypeFactory> types(new TypeFactory(ast_zone_));
    std::vector<std::string> builtin_modules = {"base"};

    auto scope = new (ast_zone_) Scope(nullptr, GLOBAL_SCOPE, ast_zone_);
    auto all_units = Compiler::ParseProject(project_dir, "main", builtin_modules,
                                            search_path_, sfs.get(), types.get(),
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
    ObjectExtraFactory extra_factory(allocator_);
    Compiler::AstEmitToBitCode(all_modules_, p_global_, o_global_, types.get(),
                               gc_, &extra_factory, function_register_, &info);
    DLOG(INFO) << "pg: " << info.global_primitive_segment_bytes << "\n"
               << "og: " << info.global_object_segment_bytes;

    type_info_base_  = info.all_type_base;
    type_void_index  = info.void_type_index;
    type_error_index = info.error_type_index;

    auto nafn = &kRtNaFn[0];
    while (nafn->name != nullptr) {
        function_register_->RegisterNativeFunction(nafn->name, nafn->pointer);
        ++nafn;
    }
    return true;
}

int VM::Run() {
    auto entry = DCHECK_NOTNULL(function_register_)->FindOrNull("::main::bootstrap");
    if (!entry) {
        LOG(ERROR) << "main function not found!";
        return -1;
    }

    auto main_ob = make_handle(o_global_->Get<HeapObject *>(entry->offset()));
    auto main_fn = main_ob->AsNormalFunction();
    if (!main_fn) {
        LOG(ERROR) << "::main::main symbol is not function!";
        return -1;
    }

    bool ok = true;
    main_thread_->Execute(main_fn, &ok);
    return main_thread_->exit_code();
}

void VM::DisassembleAll(TextOutputStream *stream) {
    std::vector<Handle<MIONormalFunction>> all_functions;
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

void VM::PrintBackstrace(std::string *buf) {
    MemoryOutputStream stream(buf);
    PrintBackstream(&stream);
}

void VM::PrintBackstream(TextOutputStream *stream) {
    for (const auto &layout : backtrace_) {
        auto fn = layout.function_object;

        if (fn->IsClosure()) {
            fn = fn->AsClosure()->GetFunction();
        }

        stream->Printf("%s() ", fn->GetName()->GetData());
        if (fn->IsNativeFunction()) {
            stream->Printf("[native %p]", fn->AsNativeFunction()->GetNativePointer());
        } else {
            auto info = fn->AsNormalFunction()->GetDebugInfo();
            if (!info) {
                continue;
            }

            bool ok = true;
            auto line = source_position_dict_->GetLine(info->file_name, layout.position, &ok);
            if (!ok) {
                stream->Printf("at %s(position:%d)", info->file_name, layout.position);
            } else {
                stream->Printf("at %s:%d:%d", info->file_name, line.line + 1, line.row + 1);
            }
        }
        stream->Write("\n", 1);
    }
}

} // namespace mio
