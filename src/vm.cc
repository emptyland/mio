#include "vm.h"
#include "vm-memory-segment.h"
#include "vm-thread.h"
#include "vm-objects.h"

namespace mio {

VM::VM()
    : main_thread_(new Thread(this))
    , code_(new MemorySegment()) {
}

VM::~VM() {
    delete code_;
    delete main_thread_;
}

Local<MIOString> VM::CreateString(const char *z, int n) {
    // TODO:
    auto total_size = n + 1 + MIOString::kDataOffset;
    auto obj = static_cast<MIOString *>(malloc(total_size));
    obj->SetLength(n);
    memcpy(obj->mutable_data(), z, n);
    obj->mutable_data()[n] = '\0';

    return Local<MIOString>(obj);
}

Local<MIONativeFunction> VM::CreateNativeFunction(const char *signature,
                                                  MIOFunctionPrototype pointer) {
    // TODO:
    Local<MIOString> sign(CreateString(signature,
                                       static_cast<int>(strlen(signature))));
    auto obj = static_cast<MIONativeFunction *>(malloc(MIONativeFunction::kMIONativeFunctionOffset));
    obj->SetSignature(sign.get());
    obj->SetNativePointer(pointer);
    return Local<MIONativeFunction>(obj);
}

Local<MIONormalFunction> VM::CreateNormalFunction(int address) {
    // TODO:
    auto obj = static_cast<MIONormalFunction *>(malloc(MIONormalFunction::kMIONormalFunctionOffset));
    obj->SetAddress(address);
    return Local<MIONormalFunction>(obj);
}

} // namespace mio
