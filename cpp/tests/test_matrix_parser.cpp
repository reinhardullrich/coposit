#include <gtest/gtest.h>

#include <coposit/parsers/matrix_parser.hpp>

using namespace coposit;

TEST(MatrixParserTest, DispatchesFracessaAndMatrixMarketInputs)
{
    const matrix_integer fracessa = parsers::matrix_parser::parse("2#1/2,0,5e-1");
    const matrix_integer matrix_market = parsers::matrix_parser::parse(
        "  %%MatrixMarket matrix array real symmetric\n"
        "2 2\n.5\n0\n5e-1\n");

    EXPECT_EQ(fracessa(0, 0).compare(matrix_market(0, 0)), 0);
    EXPECT_EQ(fracessa(0, 1).compare(matrix_market(0, 1)), 0);
    EXPECT_EQ(fracessa(1, 1).compare(matrix_market(1, 1)), 0);
}
