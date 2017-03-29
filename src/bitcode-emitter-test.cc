#include "bitcode-emitter.h"
#include "zone.h"
#include "simple-file-system.h"
#include "types.h"
#include "scopes.h"
#include "ast.h"
#include "memory-output-stream.h"
#include "vm-memory-segment.h"
#include "vm-bitcode-disassembler.h"
#include "vm-bitcode-builder.h"
#include "malloced-object-factory.h"
#include "simple-function-register.h"
#include "checker.h"
#include "compiler.h"
#include "gtest/gtest.h"

namespace mio {

class BitCodeEmitterTest : public ::testing::Test {
public:
    BitCodeEmitterTest() : sfs_(CreatePlatformSimpleFileSystem()) {}

    virtual void SetUp() override {
        zone_ = new Zone();
        global_ = new (zone_) Scope(nullptr, GLOBAL_SCOPE, zone_);
        types_ = new TypeFactory(zone_);
        factory_ = new AstNodeFactory(zone_);
        code_ = new MemorySegment();
        constant_ = new MemorySegment();
        p_global_ = new MemorySegment();
        o_global_ = new MemorySegment();
        object_factory_ = new MallocedObjectFactory();
        function_register_ = new SimpleFunctionRegister(o_global_);
    }

    virtual void TearDown() override {
        delete function_register_;
        delete object_factory_;
        delete o_global_;
        delete p_global_;
        delete constant_;
        delete code_;
        delete factory_;
        delete types_;
        delete zone_;
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


        BitCodeEmitter emitter(code_,
                               constant_,
                               p_global_,
                               o_global_,
                               types_,
                               object_factory_,
                               function_register_);
        CompiledModuleMap::Iterator m_iter(checker.mutable_all_modules());
        for (m_iter.Init(); m_iter.HasNext(); m_iter.MoveNext()) {

            CompiledUnitMap::Iterator u_iter(m_iter->value());
            for (u_iter.Init(); u_iter.HasNext(); u_iter.MoveNext()) {
                emitter.Run(m_iter->key(), u_iter->key(), u_iter->value());
            }
        }
        FunctionInfoMap info;
        function_register_->MakeFunctionInfo(&info);

        MemoryOutputStream stream(text);
        BitCodeDisassembler dasm(code_, emitter.builder()->pc(), &info, &stream);
        dasm.Run();
    }

protected:
    Zone             *zone_    = nullptr;
    Scope            *global_  = nullptr;
    TypeFactory      *types_   = nullptr;
    AstNodeFactory   *factory_ = nullptr;
    SimpleFileSystem *sfs_     = nullptr;
    MemorySegment    *code_    = nullptr;
    MemorySegment    *constant_ = nullptr;
    MemorySegment    *p_global_ = nullptr;
    MemorySegment    *o_global_ = nullptr;
    MallocedObjectFactory  *object_factory_ = nullptr;
    SimpleFunctionRegister *function_register_ = nullptr;
};

TEST_F(BitCodeEmitterTest, Sanity) {
    std::string dasm;
    ParseProject("006", &dasm);

    printf("%s\n", dasm.c_str());

//    const char z[] =
//    "[000] frame +0 +0\n"
//    "[001] ret \n";
//
//    EXPECT_STREQ(z, dasm.c_str());
}

TEST_F(BitCodeEmitterTest, ObjectCreation) {

    auto func = object_factory_->CreateNormalFunction(1);
    ASSERT_EQ(HeapObject::kNormalFunction, func->GetKind());
}

} // namespace mio
