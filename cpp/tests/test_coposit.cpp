#include <coposit/coposit.hpp>

#include <gtest/gtest.h>

#include <initializer_list>
#include <stdexcept>

namespace coposit {
namespace {

matrix_integer symmetric_matrix(size_t dimension, std::initializer_list<slong> upper_triangle)
{
    matrix_integer matrix(dimension, dimension);
    auto value = upper_triangle.begin();
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = row; column < dimension; ++column, ++value) {
            fmpz_set_si(matrix(row, column).native_handle(), *value);
            fmpz_set_si(matrix(column, row).native_handle(), *value);
        }
    }
    return matrix;
}

TEST(CopositPublicApiTests, UsesTheIncumbentAndPreservesSelectedMode)
{
    EXPECT_EQ(incumbent_model, "improved_nbc_x6");

    const matrix_integer boundary = symmetric_matrix(2, {1, -1, 1});
    const copositivity_result copositive = check(boundary, copositivity_mode::copositive);
    EXPECT_EQ(copositive.is_copositive, true);
    EXPECT_FALSE(copositive.is_strictly_copositive.has_value());

    const copositivity_result strict = check(boundary, copositivity_mode::strictly_copositive);
    EXPECT_FALSE(strict.is_copositive.has_value());
    EXPECT_EQ(strict.is_strictly_copositive, false);

    const copositivity_result both = check(boundary);
    EXPECT_EQ(both.is_copositive, true);
    EXPECT_EQ(both.is_strictly_copositive, false);
}

TEST(CopositPublicApiTests, RejectsInvalidDirectCppInput)
{
    EXPECT_THROW(check(matrix_integer()), std::invalid_argument);
    EXPECT_THROW(check(matrix_integer(2, 3)), std::invalid_argument);

    matrix_integer asymmetric(2, 2);
    asymmetric(0, 0).set_one();
    asymmetric(1, 1).set_one();
    asymmetric(0, 1).set_one();
    EXPECT_THROW(check(asymmetric), std::invalid_argument);

    const matrix_integer valid = symmetric_matrix(1, {1});
    EXPECT_THROW(check(valid, static_cast<copositivity_mode>(99)), std::invalid_argument);
}

} // namespace
} // namespace coposit
