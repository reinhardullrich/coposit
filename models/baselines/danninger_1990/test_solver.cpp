#include <gtest/gtest.h>

#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>

#include "../source_diagnostics.hpp"

#include <cstddef>
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

} // namespace

TEST(Danninger1990ModelTest, RetainsExtractedExactRegressions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {4, -1, 1, -1, 4, -1, 1, 4, -1, 4})));
    EXPECT_FALSE(model::solve(symmetric_matrix(5, {1, -1, 1, 2, -3, 2, -3, -3, 4, 5, 6, -4, 5, -8, 16})));
    EXPECT_TRUE(model::solve(symmetric_matrix(5, {3, -2, 3, 3, -2, 3, -2, 3, 3, 3, -2, 3, 3, -2, 3})));
}

TEST(Danninger1990ModelTest, SolvesPositiveDefiniteTridiagonalStressMatrix)
{
    constexpr size_t dimension = 15;
    matrix_integer matrix(dimension, dimension);
    for (size_t i = 0; i < dimension; ++i) {
        matrix(i, i) = integer((i & 1U) == 0 ? 2 : 8);
        if (i + 1 < dimension) {
            matrix(i, i + 1) = integer(-2);
            matrix(i + 1, i) = integer(-2);
        }
    }
    EXPECT_TRUE(model::solve(matrix));
}

TEST(Danninger1990ModelTest, SourceTracePreservesPlusBeforeMinusStaircases)
{
    baseline_source_diagnostics::clear();
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {4, -1, 1, -1, 4, -1, 1, 4, -1, 4})));
    const std::vector<baseline_source_diagnostics::event> expected{{"plus", 1, 2}, {"minus", 1, 2}};
    EXPECT_EQ(baseline_source_diagnostics::events, expected);
}

TEST(Danninger1990ModelTest, HasNoFormerDimensionLimit)
{
    matrix_integer matrix;
    matrix.set_identity(70);
    EXPECT_TRUE(model::solve(matrix));
}

TEST(Danninger1990ModelTest, HighOrderGeneralizedHornStopsAtTheOpenNodeLimit)
{
    constexpr size_t dimension = 999;
    matrix_integer matrix(dimension, dimension);
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = row; column < dimension; ++column) {
            const bool cycle_neighbors = column == row + 1 || (row == 0 && column + 1 == dimension);
            matrix(row, column) = integer(cycle_neighbors ? -1 : 1);
            matrix(column, row) = matrix(row, column);
        }
    }

    constexpr auto copositive = model::copositivity_mode::copositive;
    EXPECT_THROW(model::solve(matrix, copositive), open_node_limit_reached);
}

TEST(Danninger1990ModelTest, DecidesCopositivity)
{
    constexpr auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {0, 1, 1, 1, 1, 0, 0, 1, 0, 1}), copositive));
}

TEST(Danninger1990ModelTest, ClassifiesBothPredicatesInOneTraversal)
{
    const model::copositivity_classification strict =
        model::classify(symmetric_matrix(4, {5, -1, -1, -1, 5, -1, -1, 5, -1, 5}));
    EXPECT_TRUE(strict.is_copositive);
    EXPECT_TRUE(strict.is_strictly_copositive);

    const model::copositivity_classification boundary =
        model::classify(symmetric_matrix(4, {3, -1, -1, -1, 3, -1, -1, 3, -1, 3}));
    EXPECT_TRUE(boundary.is_copositive);
    EXPECT_FALSE(boundary.is_strictly_copositive);

    const model::copositivity_classification negative =
        model::classify(symmetric_matrix(4, {2, -1, -1, -1, 2, -1, -1, 2, -1, 2}));
    EXPECT_FALSE(negative.is_copositive);
    EXPECT_FALSE(negative.is_strictly_copositive);
}
