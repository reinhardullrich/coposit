#include <gtest/gtest.h>

#include <coposit/parsers/fracessa_matrix_parser.hpp>

using namespace coposit;

TEST(FracessaFormatTest, ParsesExactIntegerUpperTriangle)
{
    const parsers::parsed_matrix parsed = parsers::fracessa_matrix_parser::parse("2#123456789012345678901234567890,-1,2");
    const matrix_integer& matrix = parsed.matrix;
    EXPECT_EQ(matrix.rows(), 2U);
    EXPECT_EQ(matrix(0, 1).compare(matrix(1, 0)), 0);
    EXPECT_EQ(matrix(0, 1).sign(), -1);
    EXPECT_TRUE(parsed.denominator.is_one());
    EXPECT_FALSE(parsed.compact_circular);
}

TEST(FracessaFormatTest, RejectsWhitespace)
{
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse(" 2#1,-1,1"), std::invalid_argument);
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse("2#1, -1,1"), std::invalid_argument);
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse("2#1,-1,1\n"), std::invalid_argument);
}

TEST(FracessaFormatTest, RejectsInvalidFractionAndWrongValueCount)
{
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse("2#1,1/0,1"), std::invalid_argument);
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse("2#1,1/-2,1"), std::invalid_argument);
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse("2#1,-1/-2,1"), std::invalid_argument);
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse("2#1,1/2/3,1"), std::invalid_argument);
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse("2#1,1"), std::invalid_argument);
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse("1#bogus"), std::invalid_argument);
    EXPECT_THROW(parsers::fracessa_matrix_parser::parse("1#1 2"), std::invalid_argument);
}

TEST(FracessaFormatTest, ClearsOneMinimalFractionDecimalAndScientificDenominator)
{
    const parsers::parsed_matrix parsed = parsers::fracessa_matrix_parser::parse("2#2.5E15,+1/8,1/2");
    const matrix_integer& matrix = parsed.matrix;
    EXPECT_EQ(fmpz_cmp_si(matrix(0, 0).native_handle(), 20000000000000000L), 0);
    EXPECT_EQ(fmpz_cmp_si(matrix(0, 1).native_handle(), 1), 0);
    EXPECT_EQ(fmpz_cmp_si(matrix(1, 1).native_handle(), 4), 0);
    EXPECT_EQ(fmpz_cmp_si(parsed.denominator.native_handle(), 8), 0);
    EXPECT_FALSE(parsed.compact_circular);
}

TEST(FracessaFormatTest, ParsesCircularSymmetricForm)
{
    const parsers::parsed_matrix parsed = parsers::fracessa_matrix_parser::parse("5#1/2,-2.5e-1");
    const matrix_integer& matrix = parsed.matrix;
    EXPECT_TRUE(matrix(0, 0).is_zero());
    EXPECT_EQ(fmpz_cmp_si(matrix(0, 1).native_handle(), 2), 0);
    EXPECT_EQ(fmpz_cmp_si(matrix(0, 2).native_handle(), -1), 0);
    EXPECT_EQ(matrix(0, 2).compare(matrix(0, 3)), 0);
    EXPECT_EQ(matrix(0, 1).compare(matrix(0, 4)), 0);
    EXPECT_EQ(fmpz_cmp_si(parsed.denominator.native_handle(), 4), 0);
    EXPECT_TRUE(parsed.compact_circular);
}
