#ifndef MIO_VM_H_
#define MIO_VM_H_

#include "compiler.h"
#include "handles.h"
#include "base.h"
#include <unordered_map>

namespace mio {

template<class K, class V> class MIOHashMapStub;
template<class T> class MIOArrayStub;

class VM;
class Zone;
class Thread;
class MemorySegment;
class ManagedAllocator;
class ObjectFactory;
class GarbageCollector;
class FunctionRegister;
class TextOutputStream;
class MIOString;
class MIOVector;
class MIOReflectionType;
class SourceFilePositionDict;
class CodeCache;
class Profiler;
class TraceRecord;
struct ParsingError;

typedef int (*MIOFunctionPrototype)(VM *, Thread *);

struct BacktraceLayout {
    Handle<MIOFunction> function_object;
    Handle<MIOString>   file_name;
    int                 position;
};

class VM {
public:
    VM();
    ~VM();

    /**
     * Init VM object, must be first calling.
     */
    bool Init();

    /**
     * Compile all files of project.
     */
    bool CompileProject(const char *project_dir, ParsingError *error);

    /**
     * Run project, entry function: `::main::main'
     */
    int Run();

    DEF_GETTER(int, max_call_deep)
    DEF_PROP_RW(int, native_code_size)
    DEF_GETTER(int, tick)
    DEF_PROP_RW(std::string, gc_name)
    DEF_GETTER(std::vector<BacktraceLayout>, backtrace)
    DEF_PROP_RW(bool, jit)
    DEF_PROP_RW(int, jit_optimize)
    DEF_PROP_RW(int, hot_loop_limit)
    DEF_PTR_GETTER_NOTNULL(Thread, main_thread)
    DEF_PTR_GETTER_NOTNULL(FunctionRegister, function_register)
    DEF_PTR_GETTER_NOTNULL(GarbageCollector, gc)
    DEF_PTR_GETTER(ManagedAllocator, allocator)
    DEF_PTR_GETTER(SourceFilePositionDict, source_position_dict)

    MIOHashMapStub<Handle<MIOString>, mio_i32_t> *all_var() const {
        return DCHECK_NOTNULL(all_var_);
    }

    MIOArrayStub<Handle<MIOReflectionType>> *all_type() const {
        return DCHECK_NOTNULL(all_type_);
    }

    Thread *current() const { return DCHECK_NOTNULL(main_thread_); }

    ObjectFactory *object_factory() const {
        return reinterpret_cast<ObjectFactory *>(gc_);
    }

    void AddSerachPath(const std::string &path) { search_path_.push_back(path); }

    void DisassembleAll(TextOutputStream *stream);
    void DisassembleAll(std::string *buf);

    void PrintBackstrace(std::string *buf);
    void PrintBackstream(TextOutputStream *stream);

    Handle<MIOReflectionType> GetVoidType();
    Handle<MIOReflectionType> GetErrorType();

    friend class Thread;
    DISALLOW_IMPLICIT_CONSTRUCTORS(VM)
private:
    Handle<MIOReflectionType> EnsureGetType(int64_t tid);

    /**
     * Name of garbage collector:
     * "nogc" - The GC do nothing.
     * "msg"  - Use Mark-sweep-generation GC.
     */
    std::string gc_name_;

    /** Search path for compiling */
    std::vector<std::string> search_path_;

    /** VM execution tick */
    int tick_ = 0;

    /** Next function unique id for tracing */
    int next_function_id_ = 0;

    /** Enable/Disable just-in-time compiling */
    bool jit_ = false;

    /** just-in-time compiling optimization level */
    int jit_optimize_ = 0;

    /** How many hit loop to be hot */
    int hot_loop_limit_ = 1000;

    int max_call_deep_ = kDefaultMaxCallDeep;
    int native_code_size_ = kDefaultNativeCodeSize;
    Thread *main_thread_;
    MemorySegment *p_global_;
    MemorySegment *o_global_;
    Zone *ast_zone_;
    MIOHashMapStub<Handle<MIOString>, mio_i32_t> *all_var_ = nullptr;
    MIOArrayStub<Handle<MIOReflectionType>> *all_type_ = nullptr;
    std::unordered_map<int64_t, int> type_id2index_;
    ManagedAllocator *allocator_ = nullptr;
    CodeCache *code_cache_ = nullptr;
    GarbageCollector *gc_ = nullptr;
    FunctionRegister *function_register_ = nullptr;
    ParsedModuleMap *all_modules_ = nullptr;
    Profiler *profiler_ = nullptr;
    TraceRecord *record_ = nullptr;
    SourceFilePositionDict *source_position_dict_;
    std::vector<BacktraceLayout> backtrace_;
};

} // namespace mio

#endif // MIO_VM_H_
