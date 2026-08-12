#include <gtest/gtest.h>

#include <coposit/parsers/matrix_market_parser.hpp>

using namespace coposit;

TEST(MatrixMarketTest, ParsesExactRealSymmetricArrayInLowerColumnMajorOrder)
{
    const matrix_integer matrix = parsers::matrix_market_parser::parse(
        "%%MatrixMarket matrix array real symmetric\n"
        "% exact decimals\n"
        "2 2\n"
        "1e-1\n2.5e-1\n1\n");
    EXPECT_EQ(fmpz_cmp_si(matrix(0, 0).native_handle(), 2), 0);
    EXPECT_EQ(fmpz_cmp_si(matrix(1, 0).native_handle(), 5), 0);
    EXPECT_EQ(fmpz_cmp_si(matrix(0, 1).native_handle(), 5), 0);
    EXPECT_EQ(fmpz_cmp_si(matrix(1, 1).native_handle(), 20), 0);
}

TEST(MatrixMarketTest, ParsesIntegerSymmetricArray)
{
    const matrix_integer matrix = parsers::matrix_market_parser::parse(
        "%%MatrixMarket matrix array integer symmetric\n"
        "3 3\n1\n-2\n0\n+3\n4\n+123456789012345678901234567890\n");
    const auto expected_large = parsers::exact_number_parser::parse("123456789012345678901234567890", false, true);
    EXPECT_EQ(fmpz_cmp_si(matrix(0, 1).native_handle(), -2), 0);
    EXPECT_EQ(matrix(0, 1).compare(matrix(1, 0)), 0);
    EXPECT_EQ(fmpz_cmp_si(matrix(1, 2).native_handle(), 4), 0);
    EXPECT_EQ(matrix(2, 2).compare(expected_large.numerator), 0);
}

TEST(MatrixMarketTest, ParsesPatternCoordinateStorage)
{
    const matrix_integer pattern = parsers::matrix_market_parser::parse(
        "%%MatrixMarket matrix coordinate pattern symmetric\n"
        "3 3 3\n1 1\n2 1\n3 3\n");
    EXPECT_TRUE(pattern(0, 0).is_one());
    EXPECT_TRUE(pattern(0, 1).is_one());
    EXPECT_TRUE(pattern(1, 0).is_one());
    EXPECT_TRUE(pattern(1, 1).is_zero());

}

TEST(MatrixMarketTest, ParsesRealValuedComplexSymmetricAndRejectsImaginaryValues)
{
    const matrix_integer matrix = parsers::matrix_market_parser::parse(
        "%%MatrixMarket matrix coordinate complex symmetric\n"
        "2 2 3\n1 1 1 0\n2 1 -2.5 0e100\n2 2 3 0\n");
    EXPECT_EQ(fmpz_cmp_si(matrix(0, 0).native_handle(), 2), 0);
    EXPECT_EQ(fmpz_cmp_si(matrix(0, 1).native_handle(), -5), 0);
    EXPECT_EQ(fmpz_cmp_si(matrix(1, 1).native_handle(), 6), 0);

    EXPECT_THROW(
        parsers::matrix_market_parser::parse("%%MatrixMarket matrix coordinate complex symmetric\n1 1 1\n1 1 1 1e-30\n"),
        std::invalid_argument);
}

TEST(MatrixMarketTest, RejectsEveryNonSymmetricStructure)
{
    EXPECT_THROW(parsers::matrix_market_parser::parse("%%MatrixMarket matrix array real general\n"), std::invalid_argument);
    EXPECT_THROW(parsers::matrix_market_parser::parse("%%MatrixMarket matrix array real skew-symmetric\n"), std::invalid_argument);
    EXPECT_THROW(parsers::matrix_market_parser::parse("%%MatrixMarket matrix array complex hermitian\n"), std::invalid_argument);
}

TEST(MatrixMarketTest, RejectsEmptyOrNonsquareSymmetricMatrices)
{
    EXPECT_THROW(parsers::matrix_market_parser::parse("%%MatrixMarket matrix array integer symmetric\n0 0\n"), std::invalid_argument);
    EXPECT_THROW(parsers::matrix_market_parser::parse("%%MatrixMarket matrix array integer symmetric\n2 3\n"), std::invalid_argument);
}

TEST(MatrixMarketTest, RejectsMalformedOrAmbiguousCoordinateInput)
{
    EXPECT_THROW(
        parsers::matrix_market_parser::parse("%%MatrixMarket matrix coordinate integer symmetric\n2 2 1\n1 2 1\n"),
        std::invalid_argument);
    EXPECT_THROW(
        parsers::matrix_market_parser::parse("%%MatrixMarket matrix coordinate integer symmetric\n2 2 2\n1 1 1\n1 1 2\n"),
        std::invalid_argument);
    EXPECT_THROW(
        parsers::matrix_market_parser::parse("%%MatrixMarket matrix array pattern symmetric\n1 1\n"),
        std::invalid_argument);
    EXPECT_THROW(
        parsers::matrix_market_parser::parse("%%MatrixMarket matrix array real symmetric\n1 1\n2.5 E15\n"),
        std::invalid_argument);
    EXPECT_THROW(
        parsers::matrix_market_parser::parse("%%MatrixMarket matrix array real symmetric\n1 1\n1/2\n"),
        std::invalid_argument);
    EXPECT_THROW(
        parsers::matrix_market_parser::parse("%%MatrixMarket matrix array integer symmetric\n1 1\n1e2\n"),
        std::invalid_argument);
    EXPECT_THROW(
        parsers::matrix_market_parser::parse("%%MatrixMarket matrix array integer symmetric\n1 1\n+-1\n"),
        std::invalid_argument);
}
