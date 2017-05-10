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

    static int Panic(VM *vm, Thread *thread) {
        bool ok = true;
        auto ob = thread->GetString(0, &ok);
        DCHECK(ok);
        thread->Panic(Thread::PANIC, &ok, "%s", ob->GetData());
        thread->set_should_exit(true);
        return 0;
    }

    static int newError(VM *vm, Thread *thread);

    static int newErrorWith(VM *vm, Thread *thread);

    static int PrimitiveHash(const void *z, int n) {
        auto p = static_cast<const uint8_t *>(z);
        uint32_t h = 1315423911;
        h |= ((h << 5) + p[0] + (h >> 2));
        if (n == 1) goto result;
        h |= ((h << 5) + p[1] + (h >> 2));
        if (n == 2) goto result;
        h |= ((h << 5) + p[2] + (h >> 2));
        h |= ((h << 5) + p[3] + (h >> 2));
        if (n == 4) goto result;
        h |= ((h << 5) + p[4] + (h >> 2));
        h |= ((h << 5) + p[5] + (h >> 2));
        h |= ((h << 5) + p[6] + (h >> 2));
        h |= ((h << 5) + p[7] + (h >> 2));
    result:
        return static_cast<int>(h % 0x7FFFFFFF) ;
    }

    static bool PrimitiveEqualTo(mio_buf_t<const void> lhs, mio_buf_t<const void> rhs) {
        DCHECK_EQ(lhs.n, rhs.n);
        return memcmp(lhs.z, rhs.z, lhs.n) == 0;
    }

    static int StringHash(const void *z, int) {
        MIOString *s = *static_cast<MIOString * const*>(z);
        auto p = s->GetData();
        unsigned int h = 1315423911;
        for (int i = 0; i < s->GetLength(); ++i) {
            h ^= ((h << 5) + p[i] + (h >> 2));
        }
        return (h & 0x7FFFFFFF);
    }

    static bool StringEqualTo(mio_buf_t<const void> val1, mio_buf_t<const void> val2) {
        MIOString *lhs = *static_cast<MIOString * const*>(val1.z);
        MIOString *rhs = *static_cast<MIOString * const*>(val2.z);
        if (lhs == rhs) {
            return true;
        }
        return lhs->GetLength() != rhs->GetLength() ? false :
               strcmp(lhs->GetData(), rhs->GetData()) == 0;
    }

    static int ToString(Thread *thread, TextOutputStream *stream, void *addr,
                        Handle<MIOReflectionType> reflection, bool *ok);
};

} // namespace mio

#endif // MIO_VM_RUNTIME_H_
