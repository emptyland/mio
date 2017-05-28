#include "bitcode-emitter.h"
#include "zone.h"
#include "simple-file-system.h"
#include "types.h"
#include "scopes.h"
#include "ast.h"
#include "memory-output-stream.h"
#include "vm-code-cache.h"
#include "vm-memory-segment.h"
#include "vm-bitcode-disassembler.h"
#include "vm-bitcode-builder.h"
#include "vm-object-extra-factory.h"
#include "do-nothing-garbage-collector.h"
#include "simple-function-register.h"
#include "checker.h"
#include "compiler.h"
#include "gtest/gtest.h"

namespace mio {

class BitCodeEmitterTest : public ::testing::Test {
public:
    BitCodeEmitterTest() : sfs_(CreatePlatformSimpleFileSystem()) {}

    virtual void SetUp() override {
        allocator_ = new ManagedAllocator();
        code_cache_ = new CodeCache(kDefaultNativeCodeSize);
        zone_ = new Zone();
        global_ = new (zone_) Scope(nullptr, GLOBAL_SCOPE, zone_);
        types_ = new TypeFactory(zone_);
        factory_ = new AstNodeFactory(zone_);
        code_ = new MemorySegment();
        p_global_ = new MemorySegment();
        o_global_ = new MemorySegment();
        object_factory_ = new DoNothingGarbageCollector(allocator_);
        function_register_ = new SimpleFunctionRegister(code_cache_, o_global_);
    }

    virtual void TearDown() override {
        delete function_register_;
        delete object_factory_;
        delete o_global_;
        delete p_global_;
        delete code_;
        delete factory_;
        delete types_;
        delete zone_;
        delete code_cache_;
        delete allocator_;
    }

    void ParseProject(const char *project_dir, std::string *text) {
        std::string dir("test/");
        dir.append(project_dir);

        ParsingError error;
        auto all_units = Compiler::ParseProject(dir.c_str(),
                                                sfs_,
                                                types_,
                                                global_,
                                                zone_,
                                                &error);
        ASSERT_TRUE(all_units != nullptr) << "parsing fail: " << error.ToString();

        Checker checker(types_, all_units, global_, zone_);
        ASSERT_TRUE(checker.Run()) << "checking fail: " << checker.last_error().ToString();


        ObjectExtraFactory extra_factory(allocator_);
        BitCodeEmitter emitter(p_global_,
                               o_global_,
                               types_,
                               object_factory_,
                               &extra_factory,
                               function_register_);
        emitter.Init();
        ASSERT_TRUE(emitter.Run(checker.all_modules(), nullptr));

        std::vector<Handle<MIONormalFunction>> all_functions;
        function_register_->GetAllFunctions(&all_functions);

        MemoryOutputStream stream(text);
        BitCodeDisassembler dasm(&stream);
        for (auto function : all_functions) {
            dasm.Run(function);
        }
    }

protected:
    Zone             *zone_    = nullptr;
    Scope            *global_  = nullptr;
    TypeFactory      *types_   = nullptr;
    AstNodeFactory   *factory_ = nullptr;
    SimpleFileSystem *sfs_     = nullptr;
    MemorySegment    *code_    = nullptr;
    MemorySegment    *p_global_ = nullptr;
    MemorySegment    *o_global_ = nullptr;
    DoNothingGarbageCollector  *object_factory_ = nullptr;
    SimpleFunctionRegister *function_register_ = nullptr;
    CodeCache *code_cache_ = nullptr;
    ManagedAllocator *allocator_ = nullptr;
};

TEST_F(BitCodeEmitterTest, P006_Sanity) {
    std::string dasm;
    ParseProject("006", &dasm);

    printf("%s\n", dasm.c_str());

//    const char z[] =
//    "[000] frame +0 +0\n"
//    "[001] ret \n";
//
//    EXPECT_STREQ(z, dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P007_Import) {
    std::string dasm;
    ParseProject("007", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P008_IfOperation) {
    std::string dasm;
    ParseProject("008", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P009_RecursiveFunctionCall) {
    std::string dasm;
    ParseProject("009", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P010_MapInitializer) {
    std::string dasm;
    ParseProject("010", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P011_MapAccessor) {
    std::string dasm;
    ParseProject("011", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P012_ToStringAndStrCat) {
    std::string dasm;
    ParseProject("012", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P013_UnionOperation) {
    std::string dasm;
    ParseProject("013", &dasm);

    printf("%s\n", dasm.c_str());
}

struct PrimitiveKey {
    uint8_t size;
    uint8_t padding0;
    uint8_t data[8];

#define KEY_CREATOR(byte, bit) \
    static PrimitiveKey FromI##bit(mio_i##bit##_t value) { \
        PrimitiveKey k; \
        k.size = byte; \
        memcpy(k.data, &value, k.size); \
        return k; \
    }

    MIO_INT_BYTES_TO_BITS(KEY_CREATOR)

#undef KEY_CREATOR
};

struct PrimitiveKeyFallbackHash {
    std::size_t operator()(PrimitiveKey const &k) const {
        std::size_t h = 1315423911;
        for (int i = 0; i < k.size; ++i) {
            h ^= ((h << 5) + k.data[i] + (h >> 2));
        }
        return h;
    }
};

struct PrimitiveKeyFastHash {
    std::size_t operator()(PrimitiveKey const &k) const {
        std::size_t h = 1315423911;
        h ^= ((h << 5) + k.data[0] + (h >> 2));
        if (k.size == 1) {
            return h;
        }
        h ^= ((h << 5) + k.data[1] + (h >> 2));
        if (k.size == 2) {
            return h;
        }
        h ^= ((h << 5) + k.data[2] + (h >> 2));
        h ^= ((h << 5) + k.data[3] + (h >> 2));
        if (k.size == 4) {
            return h;
        }
        h ^= ((h << 5) + k.data[4] + (h >> 2));
        h ^= ((h << 5) + k.data[5] + (h >> 2));
        h ^= ((h << 5) + k.data[6] + (h >> 2));
        h ^= ((h << 5) + k.data[7] + (h >> 2));
        return h;
    }
};

struct PrimitiveKeyEqualTo {
    bool operator() (const PrimitiveKey &lhs, const PrimitiveKey &rhs) const {
        if (lhs.size == rhs.size) {
            return memcmp(lhs.data, rhs.data, lhs.size) == 0;
        }
        return false;
    }
};

typedef std::unordered_map<PrimitiveKey, int, PrimitiveKeyFastHash, PrimitiveKeyEqualTo> PrimitiveMap;

TEST_F(BitCodeEmitterTest, PrimitiveHashKey) {
    printf("sizeof(PrimitiveKey) = %lu\n", sizeof(PrimitiveKey));


    PrimitiveKeyFallbackHash fallback;
    PrimitiveKeyFastHash     fast;

    auto k = PrimitiveKey::FromI8(100);
    ASSERT_EQ(fallback(k), fast(k));

    k = PrimitiveKey::FromI16(0x3fff);
    ASSERT_EQ(fallback(k), fast(k));

    k = PrimitiveKey::FromI32(0x7fffffff);
    ASSERT_EQ(fallback(k), fast(k));

    PrimitiveMap map;
    auto k100 = PrimitiveKey::FromI8(100);
    map.emplace(k100, 100);

    auto k110 = PrimitiveKey::FromI32(110);
    map.emplace(k110, 110);

    ASSERT_EQ(100, map[k100]);
    ASSERT_EQ(110, map[k110]);
}

TEST_F(BitCodeEmitterTest, P014_LocalFuckingFunction) {
    std::string dasm;
    ParseProject("014", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P015_HashMapForeach) {
    std::string dasm;
    ParseProject("015", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P016_UnionTypeMatch) {
    std::string dasm;
    ParseProject("016", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P018_ArrayOperation) {
    std::string dasm;
    ParseProject("018", &dasm);

    printf("%s\n", dasm.c_str());
}

TEST_F(BitCodeEmitterTest, P019_NumericCast) {
    std::string dasm;
    ParseProject("019", &dasm);

    printf("%s\n", dasm.c_str());
}

} // namespace mio
