#include "vm-runtime.h"
#include "text-output-stream.h"

namespace mio {

const RtNativeFunctionEntry kRtNaFn[] = {
    // base library
    { "::base::print",  &NativeBaseLibrary::Print,  },
    { "::base::tick",   &NativeBaseLibrary::Tick,   },
    { "::base::gc",     &NativeBaseLibrary::GC,     },
    { "::base::fullGC", &NativeBaseLibrary::FullGC, },
    { "::base::panic",  &NativeBaseLibrary::Panic,  },

    { .name = nullptr, .pointer = nullptr, } // end of functions
};

/* static */
int NativeBaseLibrary::ToString(TextOutputStream *stream, void *addr,
                                Handle<MIOReflectionType> reflection, bool *ok) {
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
            return ToString(stream, ob->GetMutableData(), make_handle(ob->GetTypeInfo()), ok);
        } break;

        case HeapObject::kReflectionString: {
            auto ob = make_handle<MIOString>(*static_cast<MIOString **>(addr));
            return stream->Write(ob->GetData(), ob->GetLength());
        } break;

        case HeapObject::kReflectionVoid:
            return stream->Write("[void]", 6);

        case HeapObject::kReflectionError: {
            auto ob = make_handle<MIOError>(*static_cast<MIOError **>(addr));
            stream->Write("[error: ", 8);
            stream->Write(ob->GetMessage()->GetData(), ob->GetMessage()->GetLength());
            if (ob->GetLinkedError()) {
                auto link = ob->GetLinkedError();
                ToString(stream, &link, reflection, ok);
                if (!*ok) {
                    return 0;
                }
            }
            stream->Write("]", 1);
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

} // namespace mio
