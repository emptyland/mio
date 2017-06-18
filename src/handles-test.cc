#include "handles.h"
#include "vm-objects.h"
#include "gtest/gtest.h"
#include <atomic>
#include <thread>
#include <future>

namespace mio {

TEST(HandlesTest, AtomicOps) {
    std::atomic<uint32_t> val(1);

    uint32_t nval = 1;
    ASSERT_TRUE(val.compare_exchange_strong(nval, 0));
    ASSERT_EQ(0, val.load());
    ASSERT_EQ(1, nval);

    ASSERT_FALSE(val.compare_exchange_strong(nval, 1));
    nval = 0;
    ASSERT_TRUE(val.compare_exchange_strong(nval, 1));
}

TEST(HandlesTest, CocurrentDropGrab) {
    auto ob = static_cast<HeapObject *>(::malloc(HeapObject::kHeapObjectOffset));
    ob->Init(HeapObject::kExternal);

    ASSERT_EQ(0, ob->GetHandleCount());
    auto grab_result = std::async(std::launch::async, [](HeapObject *ob, int n) {
        for (int i = 0; i < n; ++i) {
            ob->Grab();
        }
    }, ob, 10000);
    auto drop_result = std::async(std::launch::async, [](HeapObject *ob, int n) {
        for (int i = 0; i < n; ++i) {
            ob->Drop();
        }
    }, ob, 10000);

    grab_result.get();
    drop_result.get();
    ASSERT_EQ(0, ob->GetHandleCount());

    ::free(ob);
}

TEST(HandlesTest, AtomicHandle) {
    auto ob = static_cast<MIOExternal *>(::malloc(MIOExternal::kMIOExternalOffset));
    ob->Init(HeapObject::kExternal);
    ASSERT_EQ(0, ob->GetHandleCount());

    {
        auto handle = make_handle(ob);
        AtomicHandle<MIOExternal> aob1(handle);
        AtomicHandle<HeapObject>  aob2(aob1);
        ASSERT_EQ(ob, handle.get());
        ASSERT_EQ(ob, aob1.get());
        ASSERT_EQ(ob, aob2.get());
        ASSERT_EQ(3, ob->GetHandleCount());
    }
    ASSERT_EQ(0, ob->GetHandleCount());
    ::free(ob);
}

} // namespace mio
