#include "vm.h"
#include "vm-memory-segment.h"
#include "vm-code-cache.h"
#include "vm-thread.h"
#include "vm-object-extra-factory.h"
#include "vm-objects.h"
#include "vm-bitcode-disassembler.h"
#include "vm-runtime.h"
#include "vm-profiler.h"
#include "fallback-managed-allocator.h"
#include "zone.h"
#include "simple-file-system.h"
#include "types.h"
#include "scopes.h"
#include "memory-output-stream.h"
#include "simple-function-register.h"
#include "do-nothing-garbage-collector.h"
#include "msg-garbage-collector.h"
#include "source-file-position-dict.h"
#include "tracing.h"

namespace mio {

VM::VM()
    : gc_name_("msg")
    , main_thread_(new Thread(this))
    , p_global_(new MemorySegment())
    , o_global_(new MemorySegment())
    , ast_zone_(new Zone())
    , source_position_dict_(new SourceFilePositionDict()) {
}

VM::~VM() {
    delete source_position_dict_;
    delete p_global_;
    delete o_global_;
    delete profiler_;
    delete main_thread_;

    if (all_var_) {
        all_var_->Drop();
    }
    delete gc_;
    if (allocator_) {
        allocator_->Finialize();
    }
    delete record_;
    delete allocator_;
    delete code_cache_;
}

bool VM::Init() {

    DCHECK_GT(native_code_size_, kPageSize);
    code_cache_ = new CodeCache(native_code_size_);
    if (!code_cache_->Init()) {
        DLOG(ERROR) << "native code cache initialize fail!";
        return false;
    }

    allocator_ = new FallbackManagedAllocator(false);
    if (!allocator_->Init()) {
        DLOG(ERROR) << "allocator init fail!";
        delete allocator_;
        allocator_ = nullptr;
        return false;
    }

    if (gc_name_.compare("msg") == 0) {
        gc_ = new MSGGarbageCollector(allocator_, code_cache_, o_global_,
                                      main_thread_, false);
    } else if (gc_name_.compare("nogc") == 0) {
        gc_ = new DoNothingGarbageCollector(allocator_);
    } else {
        DLOG(ERROR) << "bad gc name: " << gc_name_;
        return false;
    }

    function_register_ = new SimpleFunctionRegister(code_cache_, o_global_);

    if (jit_) {
        record_ = new TraceRecord(allocator_);
    }

    if (jit_optimize_ > 0) {
//        profiler_ = new Profiler(this, 17);
//        profiler_->set_sample_rate(10);
    }

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
        bool ok = true;
        auto line = source_position_dict_->GetLine(error->file_name.c_str(),
                                                   error->position, &ok);
        if (ok) {
            error->line   = line.line;
            error->column = line.column;
        }
        return false;
    }

    CompiledInfo info;
    ObjectExtraFactory extra_factory(allocator_);
    Compiler::AstEmitToBitCode(all_modules_, p_global_, o_global_, types.get(),
                               gc_, &extra_factory, function_register_, &info,
                               next_function_id_);
    DLOG(INFO) << "pg: " << info.global_primitive_segment_bytes << "\n"
               << "og: " << info.global_object_segment_bytes;

    type_info_base_   = info.all_type_base;
    type_void_index_  = info.void_type_index;
    type_error_index_ = info.error_type_index;
    all_var_          = DCHECK_NOTNULL(info.all_var);
    next_function_id_ = info.next_function_id;
    all_var_->Grab();

    if (jit_) {
        DCHECK_NOTNULL(record_)->ResizeRecord(next_function_id_);
    }

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
    if (profiler_) {
        profiler_->Start();
    }
    main_thread_->Execute(main_fn, &ok);
    if (profiler_) {
        profiler_->Stop();
        profiler_->TEST_PrintSamples();
    }
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

        if (fn->GetName()) {
            stream->Printf("%s() ", fn->GetName()->GetData());
        } else {
            stream->Printf("%p() ", fn.get());
        }
        if (fn->IsNativeFunction()) {
            stream->Printf("[native %p]", fn->AsNativeFunction()->GetNativePointer());
        } else {
            auto info = fn->AsNormalFunction()->GetDebugInfo();
            if (!info) {
                continue;
            }

            bool ok = true;
            auto line = source_position_dict_->GetLine(info->file_name,
                                                       layout.position, &ok);
            if (!ok) {
                stream->Printf("at %s(position:%d)", info->file_name,
                               layout.position);
            } else {
                stream->Printf("at %s:%d:%d", info->file_name, line.line + 1,
                               line.column + 1);
            }
        }
        stream->Write("\n", 1);
    }
}

} // namespace mio
