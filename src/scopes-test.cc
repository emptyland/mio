#include "scopes.h"
#include "zone.h"
#include "ast.h"
#include "types.h"
#include "gtest/gtest.h"


namespace mio {

#define L(z) RawString::Create((z), zone_)

class ScopeTest : public ::testing::Test {
public:

    virtual void SetUp() override {
        zone_ = new Zone();
        global_ = new (zone_) Scope(nullptr, GLOBAL_SCOPE, zone_);
        global_->set_name(L("global"));
        types_ = new TypeFactory(zone_);
        factory_ = new AstNodeFactory(zone_);
    }

    virtual void TearDown() override {
        delete factory_;
        delete types_;
        delete zone_;
    }

    Variable *DeclareSimpleVal(const char *name, Type *type, Scope *scope) {
        auto raw_name = L(name);
        auto declaration = factory_->CreateValDeclaration(raw_name->ToString(),
                                                          false,
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
}; // class ScopeTest

TEST_F(ScopeTest, Declaring) {
    auto scope = new (zone_) Scope(global_, MODULE_SCOPE, zone_);
    auto var = DeclareSimpleVal("foo", types_->GetI64(), scope);
    ASSERT_TRUE(var != nullptr);
    EXPECT_TRUE(var->is_read_only());
    EXPECT_EQ(scope, var->scope());

    auto found = scope->FindOrNullLocal(L("foo"));
    EXPECT_EQ(found, var);

    Scope *owned;
    found = global_->FindOrNullDownTo(L("foo"), &owned);
    EXPECT_EQ(found, var);
    EXPECT_EQ(owned, scope);
}

TEST_F(ScopeTest, MergeInnerScopes) {

    auto module = new (zone_) Scope(global_, MODULE_SCOPE, zone_);
    module->set_name(L("main"));

    auto unit = new (zone_) Scope(module, UNIT_SCOPE, zone_);
    unit->set_name(L("main/1.mio"));
    auto var = DeclareSimpleVal("foo", types_->GetI32(), unit);

    unit = new (zone_) Scope(module, UNIT_SCOPE, zone_);
    unit->set_name(L("main/2.mio"));
    var = DeclareSimpleVal("bar", types_->GetI64(), unit);

    ASSERT_TRUE(module->FindOrNullLocal(L("foo")) == nullptr);
    ASSERT_TRUE(module->FindOrNullLocal(L("bar")) == nullptr);

    module->MergeInnerScopes();

    var = module->FindOrNullLocal(L("foo"));
    ASSERT_TRUE(var != nullptr);
    EXPECT_EQ(types_->GetI32(), var->type());

    var = module->FindOrNullLocal(L("bar"));
    ASSERT_TRUE(var != nullptr);
    EXPECT_EQ(types_->GetI64(), var->type());
}

TEST_F(ScopeTest, MergeInnerScopesConflict) {

    auto module = new (zone_) Scope(global_, MODULE_SCOPE, zone_);
    module->set_name(L("main"));

    auto unit = new (zone_) Scope(module, UNIT_SCOPE, zone_);
    unit->set_name(L("main/1.mio"));
    auto var1 = DeclareSimpleVal("foo", types_->GetI32(), unit);

    unit = new (zone_) Scope(module, UNIT_SCOPE, zone_);
    unit->set_name(L("main/2.mio"));
    auto var2 = DeclareSimpleVal("foo", types_->GetI64(), unit);

    ASSERT_TRUE(module->FindOrNullLocal(L("foo")) == nullptr);

    Scope::MergingConflicts conflicts;
    ASSERT_FALSE(module->MergeInnerScopes(&conflicts));
    ASSERT_EQ(1, conflicts.size());

    auto found = conflicts.find("foo");
    ASSERT_TRUE(found != conflicts.end());
    ASSERT_EQ(2, found->second.size());
    EXPECT_EQ(var1, found->second[0]);
    auto scope = found->second[0]->scope();
    EXPECT_EQ(UNIT_SCOPE, scope->type());
    EXPECT_STREQ("main/1.mio", scope->name()->c_str());

    EXPECT_EQ(var2, found->second[1]);
    scope = found->second[1]->scope();
    EXPECT_EQ(UNIT_SCOPE, scope->type());
    EXPECT_STREQ("main/2.mio", scope->name()->c_str());
}

} // namespace mio
