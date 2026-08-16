#include <coposit/model.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <stdexcept>

using namespace coposit;

namespace coposit::model {
size_t level_two_skips_for_testing() noexcept;
}

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

matrix_integer path_matrix(size_t dimension)
{
    matrix_integer matrix(dimension, dimension);
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = 0; column < dimension; ++column) {
            matrix(row, column) = integer(row == column ? 10 : (row + 1 == column || column + 1 == row ? -1 : 2));
        }
    }
    return matrix;
}

TEST(ZischgFracessaModelTest, UsesLevelTwoWithoutLevelOne)
{
    EXPECT_TRUE(model::solve(path_matrix(5)));
    EXPECT_GT(model::level_two_skips_for_testing(), 0U);
}

TEST(ZischgFracessaModelTest, CombinedModeClassifiesTheBoundary)
{
    const auto result = model::classify(symmetric_matrix(4, {3, -1, -1, -1, 3, -1, -1, 3, -1, 3}));
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
}

TEST(ZischgFracessaModelTest, PreservesStrictBoundaryDecision)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(4, {3, -1, -1, -1, 3, -1, -1, 3, -1, 3})));
}

TEST(ZischgFracessaModelTest, AcceptsWhenOnlyDisconnectedKktSupportsRemain)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(5, {146, -19, -13, 24, 19, 68, 9, 9, -4, 55, 1, -14, 10, 7, 39})));
}

} // namespace
