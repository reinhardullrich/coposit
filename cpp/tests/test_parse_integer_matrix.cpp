#include <gtest/gtest.h>

#include <coposit/parse_integer_matrix.hpp>

using namespace coposit;

TEST(ParseIntegerMatrixTest, ParsesExactIntegerUpperTriangle)
{
    const matrix_integer matrix = parse_integer_matrix(" 2#123456789012345678901234567890, -1, 2\n");
    EXPECT_EQ(matrix.rows(), 2U);
    EXPECT_EQ(matrix(0, 1).compare(matrix(1, 0)), 0);
    EXPECT_EQ(matrix(0, 1).sign(), -1);
}

TEST(ParseIntegerMatrixTest, RejectsNonIntegerAndWrongValueCount)
{
    EXPECT_THROW(parse_integer_matrix("2#1,1/2,1"), std::invalid_argument);
    EXPECT_THROW(parse_integer_matrix("2#1,1"), std::invalid_argument);
}
