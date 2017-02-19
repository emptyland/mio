#include "parser.h"
#include "fixed-memory-input-stream.h"
#include "zone-hash-map.h"
#include "zone-vector.h"
#include "zone.h"
#include "ast.h"
#include "token.h"
#include "gtest/gtest.h"
#include <memory>

namespace mio {

#define T(str) RawString::Create((str), zone_)

class ParserTest : public ::testing::Test {
public:
    virtual void SetUp() override {
        zone_ = new Zone;

    }

    virtual void TearDown() override {
        delete  zone_;
        delete  input_;
    }

    void PutString(const char *z) {
        delete input_;
        input_ = new FixedMemoryInputStream(z);
    }

    Parser *CreateOnecParser(const char *z) {
        auto input = new FixedMemoryInputStream(z);
        return new Parser(zone_, input, true);
    }

    Zone *zone_ = nullptr;
    TextInputStream *input_ = nullptr;
}; // class ParserTest

TEST_F(ParserTest, Sanity) {
    std::unique_ptr<Parser> p(CreateOnecParser("package main"));

    bool ok = true;
    auto node = p->ParsePackageImporter(&ok);
    ASSERT_TRUE(ok) << p->last_error().message;
    ASSERT_TRUE(node != nullptr);
    EXPECT_STREQ("main", node->package_name()->c_str());
}

TEST_F(ParserTest, Importer) {
    std::unique_ptr<Parser> p(CreateOnecParser("package main with (\'a1\' \'a2\' \'a3\')"));

    bool ok = true;
    auto node = p->ParsePackageImporter(&ok);
    ASSERT_TRUE(ok) << p->last_error().message;
    ASSERT_TRUE(node != nullptr);

    EXPECT_STREQ("main", node->package_name()->c_str());
    EXPECT_EQ(3, node->mutable_import_list()->size());
    EXPECT_TRUE(node->mutable_import_list()->Exist(T("a1")));
    EXPECT_TRUE(node->mutable_import_list()->Exist(T("a2")));
    EXPECT_TRUE(node->mutable_import_list()->Exist(T("a3")));
}

} // namespace mio
