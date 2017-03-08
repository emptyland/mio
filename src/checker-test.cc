#include "checker.h"
#include "ast.h"
#include "zone.h"
#include "types.h"
#include "scopes.h"
#include "gtest/gtest.h"

namespace mio {

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

protected:
    Zone *zone_ = nullptr;
    Scope *global_ = nullptr;
    TypeFactory *types_ = nullptr;
    AstNodeFactory *factory_ = nullptr;
}; // class CheckerTest

#define L(z) RawString::Create((z), zone_)

TEST_F(CheckerTest, MergingSanity) {
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


} // namespace mio
