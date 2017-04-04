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
class ObjectFactory;
class FunctionRegister;
class TextOutputStream;
struct ParsingError;

typedef int (*MIOFunctionPrototype)(VM *, Thread *);

class VM {
public:
    VM();
    ~VM();

    bool Init();
    bool CompileProject(const char *project_dir, ParsingError *error);
    int Run();

    DEF_GETTER(int, max_call_deep)

    Thread *main_thread() const {
        return DCHECK_NOTNULL(main_thread_);
    }

    FunctionRegister *function_register() const {
        return DCHECK_NOTNULL(function_register_);
    }

    void DisassembleAll(TextOutputStream *stream);
    void DisassembleAll(std::string *buf);

    friend class Thread;
    DISALLOW_IMPLICIT_CONSTRUCTORS(VM)
private:
    int max_call_deep_ = kDefaultMaxCallDeep;
    Thread *main_thread_;
    MemorySegment *constants_;
    MemorySegment *p_global_;
    MemorySegment *o_global_;
    Zone *ast_zone_;
    ObjectFactory *object_factory_ = nullptr;
    FunctionRegister *function_register_ = nullptr;
    ParsedModuleMap *all_modules_ = nullptr;
};

} // namespace mio

#endif // MIO_VM_H_
