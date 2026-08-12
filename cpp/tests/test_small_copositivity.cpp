#include <coposit/small_copositivity.hpp>

#include <gtest/gtest.h>

#include <initializer_list>

using namespace coposit;

namespace {

matrix_integer symmetric_matrix(size_t dimension, std::initializer_list<slong> upper_triangle)
{
    matrix_integer matrix(dimension, dimension);
    auto value = upper_triangle.begin();
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = row; column < dimension; ++column) {
            matrix(row, column) = integer(*value++);
            matrix(column, row) = matrix(row, column);
        }
    }
    return matrix;
}

TEST(SmallCopositivityTest, DecidesStrictOrdersOneThroughThreeExactly)
{
    EXPECT_TRUE(small_copositivity::check(symmetric_matrix(1, {1})));
    EXPECT_FALSE(small_copositivity::check(symmetric_matrix(1, {0})));
    EXPECT_TRUE(small_copositivity::check(symmetric_matrix(2, {1, 0, 1})));
    EXPECT_FALSE(small_copositivity::check(symmetric_matrix(2, {1, -1, 1})));

    // Every order-two face passes, but the all-ones vector is a zero of this order-three matrix.
    EXPECT_FALSE(small_copositivity::check(symmetric_matrix(3, {2, -1, -1, 2, -1, 2})));
    EXPECT_TRUE(small_copositivity::check(symmetric_matrix(3, {3, -2, 3, 3, -2, 3})));
}

TEST(SmallCopositivityTest, ChecksAnIndexedPrincipalMatrixWithoutCopying)
{
    const matrix_integer matrix = symmetric_matrix(4, {5, 0, 0, 0, 2, -1, -1, 2, -1, 2});
    constexpr size_t indices[] = {1, 2, 3};
    EXPECT_FALSE(small_copositivity::check_principal(matrix, indices, 3));
}

TEST(SmallCopositivityTest, CopositiveModeKeepsZerosAndRejectsNegativeDirections)
{
    constexpr auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(small_copositivity::check(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(small_copositivity::check(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(small_copositivity::check(symmetric_matrix(2, {1, -2, 1}), copositive));
    EXPECT_TRUE(small_copositivity::check(symmetric_matrix(3, {2, -1, -1, 2, -1, 2}), copositive));
    EXPECT_FALSE(small_copositivity::check(symmetric_matrix(3, {1, -2, 0, 1, 0, 1}), copositive));
}

TEST(SmallCopositivityTest, RuntimeModeDispatchMatchesCompileTimeSpecializations)
{
    constexpr auto copositive = model::copositivity_mode::copositive;
    constexpr auto strict = model::copositivity_mode::strictly_copositive;
    const matrix_integer boundary = symmetric_matrix(3, {2, -1, -1, 2, -1, 2});

    EXPECT_EQ(small_copositivity::check<copositive>(boundary), small_copositivity::check(boundary, copositive));
    EXPECT_EQ(small_copositivity::check<strict>(boundary), small_copositivity::check(boundary, strict));
}

} // namespace
