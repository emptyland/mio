#ifndef MIO_VM_H_
#define MIO_VM_H_

#include "compiler.h"
#include "handles.h"
#include "base.h"
#include <map>

namespace mio {

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
class SourceFilePositionDict;
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

    bool Init();
    bool CompileProject(const char *project_dir, ParsingError *error);
    int Run();

    DEF_GETTER(int, max_call_deep)
    DEF_GETTER(int, tick);
    DEF_PROP_RW(std::string, gc_name);
    DEF_GETTER(std::vector<BacktraceLayout>, backtrace);

    Thread *main_thread() const {
        return DCHECK_NOTNULL(main_thread_);
    }

    FunctionRegister *function_register() const {
        return DCHECK_NOTNULL(function_register_);
    }

    ObjectFactory *object_factory() const {
        return reinterpret_cast<ObjectFactory *>(gc_);
    }

    GarbageCollector *gc() const {
        return DCHECK_NOTNULL(gc_);
    }

    ManagedAllocator *allocator() const {
        return allocator_;
    }

    SourceFilePositionDict *source_position_dict() const {
        return source_position_dict_;
    }

    void AddSerachPath(const std::string &path) {
        search_path_.push_back(path);
    }

    void DisassembleAll(TextOutputStream *stream);
    void DisassembleAll(std::string *buf);

    void PrintBackstrace(std::string *buf);
    void PrintBackstream(TextOutputStream *stream);

    friend class Thread;
    DISALLOW_IMPLICIT_CONSTRUCTORS(VM)
private:
    std::string gc_name_;
    std::vector<std::string> search_path_;

    int tick_ = 0;
    int max_call_deep_ = kDefaultMaxCallDeep;
    Thread *main_thread_;
    MemorySegment *p_global_;
    MemorySegment *o_global_;
    Zone *ast_zone_;
    int type_info_base_ = 0;
    int type_info_size_ = 0;
    int type_void_index = 0;
    int type_error_index = 0;
    ManagedAllocator *allocator_;
    GarbageCollector *gc_ = nullptr;
    FunctionRegister *function_register_ = nullptr;
    ParsedModuleMap *all_modules_ = nullptr;
    SourceFilePositionDict *source_position_dict_;
    std::vector<BacktraceLayout> backtrace_;
};

} // namespace mio

#endif // MIO_VM_H_
