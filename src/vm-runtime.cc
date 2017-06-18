#include "vm-runtime.h"
#include "text-output-stream.h"
#include "source-file-position-dict.h"
#include <thread>

namespace mio {

const RtNativeFunctionEntry kRtNaFn[] = {
    // base library
    { "::base::print",  &NativeBaseLibrary::Print,  },
    { "::base::tick",   &NativeBaseLibrary::Tick,   },
    { "::base::gc",     &NativeBaseLibrary::GC,     },
    { "::base::fullGC", &NativeBaseLibrary::FullGC, },
    { "::base::panic",  &NativeBaseLibrary::Panic,  },
    { "::base::newError",  &NativeBaseLibrary::NewError,  },
    { "::base::newErrorWith",  &NativeBaseLibrary::NewErrorWith,  },
    { "::base::allGlobalVariables", &NativeBaseLibrary::AllGlobalVariables, },
    { "::base::sleep", &NativeBaseLibrary::Sleep, },

    { .name = nullptr, .pointer = nullptr, } // end of functions
};

/* static */ int NativeBaseLibrary::NewError(VM *vm, Thread *thread) {
    bool ok = true;
    auto message = thread->GetString(0, &ok);
    if (!ok) {
        thread->Panic(Thread::PANIC, &ok, "incorrect argument(0), unexpected: `string\'");
        thread->set_should_exit(true);
        return -1;
    }

    auto file_name = vm->object_factory()->GetOrNewString(thread->GetSourceFileName(1));
    auto err = vm->object_factory()->CreateError(message, file_name,
                                                 thread->GetSourcePosition(1),
                                                 Handle<MIOError>());
    thread->o_stack()->Set(-kObjectReferenceSize, err.get());
    return 0;
}

/* static */ int NativeBaseLibrary::NewErrorWith(VM *vm, Thread *thread) {
    bool ok = true;
    auto message = thread->GetString(0, &ok);
    if (!ok) {
        thread->Panic(Thread::PANIC, &ok, "incorrect argument(0), unexpected: `string\'");
        thread->set_should_exit(true);
        return -1;
    }

    auto with = thread->GetError(kObjectReferenceSize, &ok);
    if (!ok) {
        thread->Panic(Thread::PANIC, &ok, "incorrect argument(1), unexpected: `error\'");
        thread->set_should_exit(true);
        return -1;
    }

    auto file_name = vm->object_factory()->GetOrNewString(thread->GetSourceFileName(1));
    auto err = vm->object_factory()->CreateError(message, file_name,
                                                 thread->GetSourcePosition(1),
                                                 with);
    thread->o_stack()->Set(-kObjectReferenceSize, err.get());
    return 0;
}

/* static */
int NativeBaseLibrary::ToString(Thread *thread, TextOutputStream *stream,
                                void *addr, Handle<MIOReflectionType> reflection,
                                bool *ok) {
    switch (reflection->GetKind()) {
        case HeapObject::kReflectionIntegral: {
            switch (reflection->AsReflectionIntegral()->GetBitWide()) {
                #define DEFINE_CASE(byte, bit) \
                    case bit: return stream->Printf("%" PRId##bit, *static_cast<mio_i##bit##_t *>(addr));
                    MIO_INT_BYTES_TO_BITS(DEFINE_CASE)
                #undef DEFINE_CASE
                default:
                    DLOG(ERROR) << "bad integral bitwide: " <<
                    reflection->AsReflectionIntegral()->GetBitWide();
                    goto fail;
            }
        } break;

        case HeapObject::kReflectionFloating:
            switch (reflection->AsReflectionFloating()->GetBitWide()) {
                #define DEFINE_CASE(byte, bit) \
                    case bit: return stream->Printf("%0.5f", *static_cast<mio_f##bit##_t *>(addr));
                    MIO_FLOAT_BYTES_TO_BITS(DEFINE_CASE)
                #undef DEFINE_CASE
                default:
                    DLOG(ERROR) << "bad floating bitwide: " <<
                    reflection->AsReflectionFloating()->GetBitWide();
                    goto fail;
            }
            break;

        case HeapObject::kReflectionUnion: {
            auto ob = make_handle<MIOUnion>(*static_cast<MIOUnion **>(addr));
            return ToString(thread, stream, ob->GetMutableData(),
                            make_handle(ob->GetTypeInfo()), ok);
        } break;

        case HeapObject::kReflectionString: {
            auto ob = make_handle<MIOString>(*static_cast<MIOString **>(addr));
            return stream->Write(ob->GetData(), ob->GetLength());
        } break;

        case HeapObject::kReflectionVoid:
            return stream->Write("[void]", 6);

        case HeapObject::kReflectionError: {
            auto ob = make_handle<MIOError>(*static_cast<MIOError **>(addr));
            stream->Write("[error] ");
            bool ok = true;
            auto line = thread->vm()->source_position_dict()->GetLine(
                    ob->GetFileName()->GetData(), ob->GetPosition(), &ok);
            if (ok) {
                stream->Printf("%s:%d:%d ", ob->GetFileName()->GetData(),
                               line.line + 1, line.column + 1);
            }
            stream->Write(ob->GetMessage()->GetData(), ob->GetMessage()->GetLength());
            if (ob->GetLinkedError()) {
                stream->Write(" ...", 4);
            }
        } break;

        case HeapObject::kReflectionMap:
        case HeapObject::kReflectionFunction:

        fail:
        default:
            *ok = false;
            break;
    }
    return 0;
}

/*static*/ int NativeBaseLibrary::Sleep(VM *vm, Thread *thread) {
    auto mils = thread->GetInt(0);
    thread->set_syscall(static_cast<int>(mils));
    std::this_thread::sleep_for(std::chrono::milliseconds(mils));
    thread->set_syscall(0);
    return 0;
}

} // namespace mio
