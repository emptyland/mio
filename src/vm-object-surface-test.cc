#include "vm-object-surface.h"
#include "vm-objects.h"
#include "vm.h"
#include "text-output-stream.h"
#include "gtest/gtest.h"

namespace mio {

class ObjectSurefaceTest : public ::testing::Test {
public:
    virtual void SetUp() override {
        vm_ = new VM();
        vm_->Init();
        factory_ = vm_->object_factory();
    }

    virtual void TearDown() override {
        delete vm_;
    }

protected:
    VM *vm_ = nullptr;
    ObjectFactory *factory_;
};

TEST_F(ObjectSurefaceTest, Sanity) {
    auto string = factory_->CreateReflectionString(0);
    auto integral = factory_->CreateReflectionIntegral(1, 64);
    auto map = factory_->CreateHashMap(0, 7, string, integral);

    ASSERT_FALSE(map.empty());
    auto key = factory_->CreateString("1st", 3);
    int64_t val = 1;

    MIOHashMapSurface surface(map.get(), vm_->allocator());
    ASSERT_TRUE(surface.RawPut(key.address(), &val));

    key = factory_->CreateString("2nd", 3);
    val = 2;
    ASSERT_TRUE(surface.RawPut(key.address(), &val));
    ASSERT_FALSE(surface.RawPut(key.address(), &val));

    ASSERT_EQ(2, *static_cast<int *>(surface.RawGet(key.address())));
}

TEST_F(ObjectSurefaceTest, Rehash) {
    auto string = factory_->CreateReflectionString(0);
    auto integral = factory_->CreateReflectionIntegral(1, 32);
    auto map = factory_->CreateHashMap(0, 7, string, integral);
    ASSERT_EQ(7, map->GetSlotSize());

    MIOHashMapStub<Handle<MIOString>, mio_i32_t> stub(map.get(), vm_->allocator());
    for (int i = 0; i < 20; ++i) {
        auto s = TextOutputStream::sprintf("k.%d", i);
        auto key = factory_->GetOrNewString(s.c_str(), static_cast<int>(s.size()));
        stub.Put(key, i);
    }
    ASSERT_EQ(map->GetSlotSize(), 18);

    for (int i = 0; i < 20; ++i) {
        auto s = TextOutputStream::sprintf("k.%d", i);
        auto key = factory_->GetOrNewString(s.c_str(), static_cast<int>(s.size()));
        ASSERT_EQ(i, stub.Get(key));
    }

    MIOHashMapStub<Handle<MIOString>, mio_i32_t>::Iterator iter(&stub);
    int counter = 0;
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        ++counter;
    }
    ASSERT_EQ(stub.core()->GetSize(), counter);
}

TEST_F(ObjectSurefaceTest, Delete) {
    auto string = factory_->CreateReflectionString(0);
    auto integral = factory_->CreateReflectionIntegral(1, 32);
    auto map = factory_->CreateHashMap(0, 7, string, integral);
    ASSERT_EQ(7, map->GetSlotSize());

    MIOHashMapStub<Handle<MIOString>, mio_i32_t> stub(map.get(), vm_->allocator());
    for (int i = 0; i < 20; ++i) {
        auto s = TextOutputStream::sprintf("k.%d", i);
        auto key = factory_->GetOrNewString(s.c_str(), static_cast<int>(s.size()));
        stub.Put(key, i);
    }
    ASSERT_EQ(18, map->GetSlotSize());
    ASSERT_EQ(20, map->GetSize());

    auto key = factory_->GetOrNewString("k.0", 3);
    ASSERT_TRUE(stub.Delete(key));
    ASSERT_EQ(19, map->GetSize());
    ASSERT_FALSE(stub.Exist(key));

    MIOHashMapStub<Handle<MIOString>, mio_i32_t>::Iterator iter(&stub);
    int counter = 0;
    for (iter.Init(); iter.HasNext(); iter.MoveNext()) {
        ++counter;
    }
    ASSERT_EQ(19, counter);
}


} // namespace mio
