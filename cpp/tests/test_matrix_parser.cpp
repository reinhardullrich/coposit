#include <gtest/gtest.h>

#include <coposit/parsers/matrix_parser.hpp>

using namespace coposit;

TEST(MatrixParserTest, DispatchesFracessaAndMatrixMarketInputs)
{
    const parsers::parsed_matrix fracessa = parsers::matrix_parser::parse("2#1/2,0,5e-1");
    const parsers::parsed_matrix matrix_market = parsers::matrix_parser::parse(
        "  %%MatrixMarket matrix array real symmetric\n"
        "2 2\n.5\n0\n5e-1\n");

    EXPECT_EQ(fracessa.matrix(0, 0).compare(matrix_market.matrix(0, 0)), 0);
    EXPECT_EQ(fracessa.matrix(0, 1).compare(matrix_market.matrix(0, 1)), 0);
    EXPECT_EQ(fracessa.matrix(1, 1).compare(matrix_market.matrix(1, 1)), 0);
    EXPECT_EQ(fracessa.denominator.compare(matrix_market.denominator), 0);
    EXPECT_FALSE(fracessa.compact_circular);
    EXPECT_FALSE(matrix_market.compact_circular);
}
