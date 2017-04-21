#ifndef MIO_VM_RUNTIME_H_
#define MIO_VM_RUNTIME_H_

#include "vm.h"
#include "vm-thread.h"
#include "vm-stack.h"
#include "vm-garbage-collector.h"
#include "base.h"

namespace mio {

struct RtNativeFunctionEntry {
    const char *          name;
    MIOFunctionPrototype  pointer;
};

extern const RtNativeFunctionEntry kRtNaFn[];

class NativeBaseLibrary {
public:
    static int Print(VM *vm, Thread *thread) {
        auto ob = thread->GetObject(0);

        if (!ob->IsString()) {
            printf("error: parameter is not string\n");
        } else {
            printf("%s", ob->AsString()->GetData());
        }
        return 0;
    }

    static int Tick(VM *vm, Thread *thread) {
        thread->p_stack()->Set(-4, vm->tick());
        return 0;
    }

    static int GC(VM *vm, Thread *thread) {
        vm->gc()->Step(vm->tick());
        return 0;
    }

    static int FullGC(VM *vm, Thread *thread) {
        vm->gc()->FullGC();
        return 0;
    }
};

} // namespace mio

#endif // MIO_VM_RUNTIME_H_
