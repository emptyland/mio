#include "nyaa-value-factory.h"
#include "gtest/gtest.h"

namespace mio {

class NValueFactoryTest : public ::testing::Test {
public:
    virtual void SetUp() override {
        zone_ = new Zone();
        factory_ = new NValueFactory(zone_);
    }

    virtual void TearDown() override {
        delete factory_;
        delete zone_;
    }

protected:
    Zone *zone_ = nullptr;
    NValueFactory *factory_;
}; // class NValueFactoryTest

TEST_F(NValueFactoryTest, Sanity) {
    //auto add = factory_->CreateAdd(NType::Int8(), <#mio::NValue *lhs#>, <#mio::NValue *rhs#>)
}

} // namespace mio
