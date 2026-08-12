#include <gtest/gtest.h>

#include <coposit/parsers/exact_number_parser.hpp>

using namespace coposit::parsers;

TEST(ExactNumberTest, ParsesAndReducesEveryAcceptedExactForm)
{
    const exact_number_parser::exact_rational scientific = exact_number_parser::parse("2.5E-3", false);
    const exact_number_parser::exact_rational negative_fraction = exact_number_parser::parse("-10/20", true);
    const exact_number_parser::exact_rational positive_fraction = exact_number_parser::parse("+3/6", true);

    EXPECT_EQ(fmpz_cmp_si(scientific.numerator.native_handle(), 1), 0);
    EXPECT_EQ(fmpz_cmp_si(scientific.denominator.native_handle(), 400), 0);
    EXPECT_EQ(fmpz_cmp_si(negative_fraction.numerator.native_handle(), -1), 0);
    EXPECT_EQ(fmpz_cmp_si(negative_fraction.denominator.native_handle(), 2), 0);
    EXPECT_EQ(fmpz_cmp_si(positive_fraction.numerator.native_handle(), 1), 0);
    EXPECT_EQ(fmpz_cmp_si(positive_fraction.denominator.native_handle(), 2), 0);
}

TEST(ExactNumberTest, EnforcesTheTwoFormatGrammars)
{
    EXPECT_THROW(exact_number_parser::parse("1/2", false), std::invalid_argument);
    EXPECT_THROW(exact_number_parser::parse("1/-2", true), std::invalid_argument);
    EXPECT_THROW(exact_number_parser::parse("-1/-2", true), std::invalid_argument);
    EXPECT_THROW(exact_number_parser::parse("1/0", true), std::invalid_argument);
    EXPECT_THROW(exact_number_parser::parse("1.5/2", true), std::invalid_argument);
    EXPECT_THROW(exact_number_parser::parse("1e2", false, true), std::invalid_argument);
}
