#include "checker.h"
#include "ast.h"
#include "zone.h"
#include "types.h"
#include "scopes.h"
#include "gtest/gtest.h"

namespace mio {

#define L(z) RawString::Create((z), zone_)

class CheckerTest : public ::testing::Test {
public:
    virtual void SetUp() override {
        zone_ = new Zone();
        global_ = new (zone_) Scope(nullptr, GLOBAL_SCOPE, zone_);
        types_ = new TypeFactory(zone_);
        factory_ = new AstNodeFactory(zone_);
    }

    virtual void TearDown() override {
        delete factory_;
        delete types_;
        delete zone_;
    }

    Variable *DeclareSimpleVal(const char *name, Type *type, Scope *scope,
                               bool is_export) {
        auto raw_name = L(name);
        auto declaration = factory_->CreateValDeclaration(raw_name->ToString(),
                                                          is_export,
                                                          type,
                                                          nullptr,
                                                          scope,
                                                          false,
                                                          0);
        return scope->Declare(raw_name, declaration);
    }

protected:
    Zone *zone_ = nullptr;
    Scope *global_ = nullptr;
    TypeFactory *types_ = nullptr;
    AstNodeFactory *factory_ = nullptr;
}; // class CheckerTest


TEST_F(CheckerTest, Sanity) {
    CompiledUnitMap *all_units = new (zone_) CompiledUnitMap(zone_);

    auto unit_stmts = new (zone_) ZoneVector<Statement *>(zone_);
    auto pkg_stmt = factory_->CreatePackageImporter("main", 0);
    auto module = new (zone_) Scope(global_, MODULE_SCOPE, zone_);
    module->set_name(L("main"));
    unit_stmts->Add(pkg_stmt);

    all_units->Put(L("main/1.mio"), unit_stmts);
    auto scope = new (zone_) Scope(module, UNIT_SCOPE, zone_);
    scope->set_name(L("main/1.mio"));

    unit_stmts = new (zone_) ZoneVector<Statement *>(zone_);
    unit_stmts->Add(pkg_stmt);

    all_units->Put(L("main/2.mio"), unit_stmts);
    scope = new (zone_) Scope(module, UNIT_SCOPE, zone_);
    scope->set_name(L("main/2.mio"));

    Checker checker(types_, all_units, global_, zone_);
    ASSERT_TRUE(checker.Run()) << checker.last_error().message;
}

TEST_F(CheckerTest, BinaryOperationTypeReduction) {
    CompiledUnitMap *all_units = new (zone_) CompiledUnitMap(zone_);

    auto module = new (zone_) Scope(global_, MODULE_SCOPE, zone_);
    module->set_name(L("main"));

    auto scope = new (zone_) Scope(module, UNIT_SCOPE, zone_);
    scope->set_name(L("main/1.mio"));

    auto unit_stmts = new (zone_) ZoneVector<Statement *>(zone_);
    auto pkg_stmt = factory_->CreatePackageImporter("main", 0);
    unit_stmts->Add(pkg_stmt);

    // val a = 1 + 1
    Expression *init = factory_->CreateBinaryOperation(OP_ADD,
                                            factory_->CreateI64SmiLiteral(1, 0),
                                            factory_->CreateI64SmiLiteral(1, 0),
                                            0);
    auto stmt = factory_->CreateValDeclaration("a", false,
                                                     types_->GetUnknown(),
                                                     init, scope, false, 0);
    scope->Declare(stmt->name(), stmt);
    unit_stmts->Add(stmt);

    // val b = -a
    auto sym = factory_->CreateSymbol("a", "", 0);
    init = factory_->CreateUnaryOperation(OP_MINUS, sym, 0);
    stmt = factory_->CreateValDeclaration("b", false, types_->GetUnknown(),
                                          init, scope, false, 0);
    scope->Declare(stmt->name(), stmt);
    unit_stmts->Add(stmt);

    all_units->Put(L("main/1.mio"), unit_stmts);

    Checker checker(types_, all_units, global_, zone_);
    ASSERT_TRUE(checker.Run()) << checker.last_error().message;

    auto var = module->FindOrNullLocal(L("a"));
    ASSERT_TRUE(var != nullptr);
    EXPECT_EQ(types_->GetI64(), var->type());

    var = module->FindOrNullLocal(L("b"));
    ASSERT_TRUE(var != nullptr);
    EXPECT_EQ(types_->GetI64(), var->type());
}

TEST_F(CheckerTest, SymbolBetweenModuleDiscovery) {
    CompiledUnitMap *all_units = new (zone_) CompiledUnitMap(zone_);

    auto unit_stmts = new (zone_) ZoneVector<Statement *>(zone_);

    // module: main
    auto pkg_stmt = factory_->CreatePackageImporter("main", 0);
    pkg_stmt->mutable_import_list()->Put(L("foo"), RawString::kEmpty);
    auto module = new (zone_) Scope(global_, MODULE_SCOPE, zone_);
    module->set_name(L("main"));
    unit_stmts->Add(pkg_stmt);
    unit_stmts->Add(factory_->CreateSymbol("id", "foo", 0));

    all_units->Put(L("main/1.mio"), unit_stmts);
    auto scope = new (zone_) Scope(module, UNIT_SCOPE, zone_);
    scope->set_name(L("main/1.mio"));

    // module: foo
    pkg_stmt = factory_->CreatePackageImporter("foo", 0);
    module = new (zone_) Scope(global_, MODULE_SCOPE, zone_);
    module->set_name(L("foo"));

    scope = new (zone_) Scope(module, UNIT_SCOPE, zone_);
    scope->set_name(L("foo/1.mio"));
    auto var = DeclareSimpleVal("id", types_->GetI64(), scope, true);

    unit_stmts = new (zone_) ZoneVector<Statement *>(zone_);
    unit_stmts->Add(pkg_stmt);
    unit_stmts->Add(var->declaration());

    all_units->Put(L("foo/1.mio"), unit_stmts);

    Checker checker(types_, all_units, global_, zone_);

    ASSERT_TRUE(checker.Run()) << checker.last_error().message;

    auto pair = checker.mutable_all_modules()->Get(L("main"))->value()->Get(L("main/1.mio"));
    auto node = pair->value()->At(1);

    ASSERT_TRUE(node->IsVariable());
    var = node->AsVariable();
    EXPECT_TRUE(var->is_read_only());
}


} // namespace mio
