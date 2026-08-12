#include <coposit/integer.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace coposit;

TEST(IntegerTest, ConvertsArbitraryWidthTextInTheRequestedBase)
{
    const std::string bits = "1" + std::string(64, '0') + "1";
    integer value;

    value.set_string(bits, 2);

    EXPECT_EQ(value.to_string(2), bits);
    EXPECT_EQ(value.to_string(), "36893488147419103233");
    EXPECT_THROW(value.set_string("2", 2), std::invalid_argument);
}
