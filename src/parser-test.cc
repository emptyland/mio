#include "parser.h"
#include "scopes.h"
#include "fixed-memory-input-stream.h"
#include "zone-hash-map.h"
#include "zone-vector.h"
#include "zone.h"
#include "types.h"
#include "ast-printer.h"
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
        global_scope_ = new (zone_) Scope(nullptr, GLOBAL_SCOPE, zone_);
        types_ = new TypeFactory(zone_);
        streams_ = new FixedMemoryStreamFactory();
    }

    virtual void TearDown() override {
        delete streams_;
        delete types_;
        delete  zone_;
    }

    Parser *CreateOnecParser(const char *z) {
        streams_->PutInputStream(":memory:", z);

        auto p = new Parser(types_, streams_, global_scope_, zone_);
        p->SwitchInputStream(":memory:");
        return p;
    }

    void AssertionToYamlAstTree(const char *z, std::string *yaml) {
        std::unique_ptr<Parser> p(CreateOnecParser(z));

        bool ok = true;
        auto node = p->ParseStatement(&ok);
        auto err = p->last_error();
        ASSERT_TRUE(ok) << err.position << ":" << err.message;
        ASSERT_TRUE(node != nullptr);

        AstPrinter::ToYamlString(node, 2, yaml);
    }

    Zone *zone_ = nullptr;
    Scope *global_scope_ = nullptr;
    TypeFactory *types_ = nullptr;
    FixedMemoryStreamFactory *streams_ = nullptr;
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

TEST_F(ParserTest, ImporterAlias) {
    std::unique_ptr<Parser> p(CreateOnecParser("package main with (\'a1\' as b1)"));

    bool ok = true;
    auto node = p->ParsePackageImporter(&ok);
    ASSERT_TRUE(ok) << p->last_error().message;
    ASSERT_TRUE(node != nullptr);

    EXPECT_STREQ("main", node->package_name()->c_str());
    EXPECT_EQ(1, node->mutable_import_list()->size());

    auto pair = node->mutable_import_list()->Get(T("a1"));
    ASSERT_TRUE(pair != nullptr);
    EXPECT_STREQ("a1", pair->key()->c_str());
    EXPECT_STREQ("b1", pair->value()->c_str());
}

TEST_F(ParserTest, Operation) {
    std::unique_ptr<Parser> p(CreateOnecParser("1 + 1"));

    bool ok = true;
    int rop = 0;
    auto node = p->ParseExpression(0, &rop, &ok);
    ASSERT_TRUE(ok) << p->last_error().message;
    ASSERT_TRUE(node != nullptr);

    EXPECT_TRUE(node->IsBinaryOperation());

    auto bin = node->AsBinaryOperation();
    EXPECT_TRUE(bin->lhs()->IsSmiLiteral());
    EXPECT_TRUE(bin->rhs()->IsSmiLiteral());

    auto lhs = bin->lhs()->AsSmiLiteral();
    EXPECT_EQ(1, lhs->i64());
    auto rhs = bin->rhs()->AsSmiLiteral();
    EXPECT_EQ(1, rhs->i64());
}

TEST_F(ParserTest, OperationProi) {
    std::unique_ptr<Parser> p(CreateOnecParser("1 + 2 * 3"));

    bool ok = true;
    int rop = 0;
    auto node = p->ParseExpression(0, &rop, &ok);
    ASSERT_TRUE(ok) << p->last_error().message;
    ASSERT_TRUE(node != nullptr);

    EXPECT_TRUE(node->IsBinaryOperation());

    auto bin = node->AsBinaryOperation();
    EXPECT_EQ(OP_ADD, bin->op());
    EXPECT_TRUE(bin->lhs()->IsSmiLiteral());
    EXPECT_TRUE(bin->rhs()->IsBinaryOperation());
}

TEST_F(ParserTest, FieldAccessing) {
    std::unique_ptr<Parser> p(CreateOnecParser("base::object.name"));

    bool ok = true;
    int rop = 0;
    auto node = p->ParseExpression(0, &rop, &ok);
    ASSERT_TRUE(ok) << p->last_error().message;
    ASSERT_TRUE(node != nullptr);

    EXPECT_TRUE(node->IsFieldAccessing());

    auto fa = node->AsFieldAccessing();
    EXPECT_STREQ("name", fa->field_name()->c_str());

    EXPECT_TRUE(fa->expression()->IsSymbol());

    auto symbol = fa->expression()->AsSymbol();
    EXPECT_TRUE(symbol->has_name_space());
    EXPECT_STREQ("object", symbol->name()->c_str());
    EXPECT_STREQ("base", symbol->name_space()->c_str());
}

TEST_F(ParserTest, Assignment) {
    std::unique_ptr<Parser> p(CreateOnecParser("i = 1"));

    bool ok = true;
    int rop = 0;
    auto node = p->ParseExpression(0, &rop, &ok);
    ASSERT_TRUE(ok) << p->last_error().message;
    ASSERT_TRUE(node != nullptr);

    EXPECT_TRUE(node->IsAssignment());

    auto assignment = node->AsAssignment();
    EXPECT_TRUE(assignment->target()->IsSymbol());
    EXPECT_STREQ("i", assignment->target()->AsSymbol()->name()->c_str());

    EXPECT_TRUE(assignment->rval()->IsSmiLiteral());
    EXPECT_EQ(1, assignment->rval()->AsSmiLiteral()->i64());
}

TEST_F(ParserTest, Calling) {
    std::string yaml;

    AssertionToYamlAstTree("base::eval(1, 2, i+1, -i)", &yaml);

    const char z[] =
    "expression: \n"
    "  symbol: eval::base\n"
    "arguments: \n"
    "  - i64: 1\n"
    "  - i64: 2\n"
    "  - op: ADD\n"
    "    lhs: \n"
    "      symbol: i\n"
    "    rhs: \n"
    "      i64: 1\n"
    "  - op: MINUS\n"
    "    operand: \n"
    "      symbol: i\n";

    EXPECT_STREQ(z, yaml.c_str());
}

TEST_F(ParserTest, IfOperationOnlyThen) {
    std::string yaml;

    AssertionToYamlAstTree("if (i < 0) 1", &yaml);

    const char z[] =
    "if: \n"
    "  op: LT\n"
    "  lhs: \n"
    "    symbol: i\n"
    "  rhs: \n"
    "    i64: 0\n"
    "then: \n"
    "  i64: 1\n";

    EXPECT_STREQ(z, yaml.c_str());
}

TEST_F(ParserTest, IfOperationHasElse) {
    std::string yaml;

    AssertionToYamlAstTree("if (i < 0) 1 else -1", &yaml);

    const char z[] =
    "if: \n"
    "  op: LT\n"
    "  lhs: \n"
    "    symbol: i\n"
    "  rhs: \n"
    "    i64: 0\n"
    "then: \n"
    "  i64: 1\n"
    "else: \n"
    "  i64: -1\n"
    ;

    EXPECT_STREQ(z, yaml.c_str());
}

TEST_F(ParserTest, IfOperationElseIf) {
    std::string yaml;

    AssertionToYamlAstTree("if (i < 0) 1 else if (i > 0) -1", &yaml);

    const char z[] =
    "if: \n"
    "  op: LT\n"
    "  lhs: \n"
    "    symbol: i\n"
    "  rhs: \n"
    "    i64: 0\n"
    "then: \n"
    "  i64: 1\n"
    "else: \n"
    "  if: \n"
    "    op: GT\n"
    "    lhs: \n"
    "      symbol: i\n"
    "    rhs: \n"
    "      i64: 0\n"
    "  then: \n"
    "    i64: -1\n"
    ;

    EXPECT_STREQ(z, yaml.c_str());
}

TEST_F(ParserTest, EmptyBlock) {
    std::string yaml;
    AssertionToYamlAstTree("{}", &yaml);

    const char z[] = "block: -EMPTY-\n";
    EXPECT_STREQ(z, yaml.c_str());
}

TEST_F(ParserTest, Block) {
    std::string yaml;
    AssertionToYamlAstTree("{\nc = a + b\nc}\n", &yaml);

    const char z[] =
    "block: \n"
    "  - taget: \n"
    "      symbol: c\n"
    "    rval: \n"
    "      op: ADD\n"
    "      lhs: \n"
    "        symbol: a\n"
    "      rhs: \n"
    "        symbol: b\n"
    "  - symbol: c\n";
    EXPECT_STREQ(z, yaml.c_str());
}

TEST_F(ParserTest, ValDeclaration) {
    std::unique_ptr<Parser> p(CreateOnecParser("val a: int = 1"));

    auto scope = p->EnterScope("mock", FUNCTION_SCOPE);
    bool ok = true;
    auto node = p->ParseStatement(&ok);
    auto err = p->last_error();
    p->LeaveScope();
    ASSERT_TRUE(ok) << err.position << ":" << err.message;
    ASSERT_TRUE(node != nullptr);

    Scope *owned = nullptr;
    auto declaration = global_scope_->FindOrNullDownTo("a", &owned);
    ASSERT_TRUE(declaration != nullptr);

    EXPECT_EQ(declaration, node);
    EXPECT_TRUE(declaration->IsValDeclaration());
    EXPECT_EQ(declaration->AsValDeclaration()->scope(), owned);
    EXPECT_EQ(scope, owned);
}

} // namespace mio
