#include "object.h"
#include "object_builder.h"
#include "gtest/gtest.h"
#include <memory>
#include <string>

namespace mio {

class ObjectTest : public ::testing::Test {
public:
    virtual void SetUp() override {
        builder_ = NewMallocObjectBuilder(true);
    }

    virtual void TearDown() override {
        delete builder_;
    }

protected:
    ObjectBuilder *builder_ = nullptr;
};

TEST_F(ObjectTest, Sanity) {
    auto num = builder_->NewNumber(100);
    ASSERT_EQ(100, num->native());
}

TEST_F(ObjectTest, ToString) {
    auto num = builder_->NewNumber(100);

    std::string s;
    num->ToString(&s);
    ASSERT_EQ("100.000000", s);

    s.clear();
    auto empty = builder_->NewPair(nullptr, nullptr);
    auto list = builder_->NewPair(num, empty);
    
    list->ToString(&s);
    ASSERT_EQ("(100.000000 )", s);
}

TEST_F(ObjectTest, ListToString) {
    // (lambda (a b) (+ a b))
    // car: lambda
    // cdr: (a b) (+ a b)
    // car: (a b)
    // cdr: (+ a b)

    auto id_lambda = builder_->NewId("lambda", 6);
    auto id_a = builder_->NewId("a", 1);
    auto id_b = builder_->NewId("b", 1);
    auto id_plus = builder_->NewId("+", 1);
    auto empty = builder_->NewPair(nullptr, nullptr);

    std::string s;
    auto expr = builder_->NewPair(id_b, empty);
    auto args = builder_->NewPair(id_a, expr);

    expr = builder_->NewPair(id_b, empty);
    expr = builder_->NewPair(id_a, expr);
    auto body = builder_->NewPair(id_plus, expr);

    expr = builder_->NewPair(body, empty);
    expr = builder_->NewPair(args, expr);
    expr = builder_->NewPair(id_lambda, expr);
    expr->ToString(&s);
    EXPECT_EQ("(lambda (a b ) (+ a b ) )", s);
}

} // namespace mio

