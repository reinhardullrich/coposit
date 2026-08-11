#include <coposit/model.hpp>

#include "../source_trace.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <stdexcept>

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

TEST(Bundfuss2008ModelTest, HandlesMinimalStrictBoundary)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
}

TEST(Bundfuss2008ModelTest, AcceptsEntrywiseNonnegativeGram)
{
    matrix_integer identity;
    identity.set_identity(70);
    EXPECT_TRUE(model::solve(identity));
}

TEST(Bundfuss2008ModelTest, UsesFractionalBundfussSplit)
{
    // The selected edge has lambda = 5/12, exercising integer denominator clearing rather than midpoint splitting.
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {2, -1, 1, 5, -2, 3})));
}

TEST(Bundfuss2008ModelTest, SourceTraceRetainsFiveTwelfthsSplit)
{
    baseline_source_trace::clear();
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {2, -1, 1, 5, -2, 3})));
    ASSERT_GE(baseline_source_trace::events.size(), 2U);
    EXPECT_EQ(baseline_source_trace::events[0], (baseline_source_trace::event{"split", 1, 2}));
    EXPECT_EQ(baseline_source_trace::events[1], (baseline_source_trace::event{"lambda", 5, 12}));
}

TEST(Bundfuss2008ModelTest, FindsNegativeDirectionAfterPartitioning)
{
    matrix_integer matrix(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) matrix(row, column) = integer(row == column ? 5 : -2);
    }
    EXPECT_FALSE(model::solve(matrix));
}

TEST(Bundfuss2008ModelTest, MatchesStrictCorpusBranch)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(
        6, {90, -45, 90, 90, -45, -45, 90, -45, -45, 90, 90, 90, 90, -45, -45, 90, -45, -45, 90, 90, 90})));
}

TEST(Bundfuss2008ModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer matrix = symmetric_matrix(3, {2, -1, 1, 5, -2, 3});
    fmpz_mat_scalar_mul_fmpz(matrix.native_handle(), matrix.native_handle(), scale.native_handle());
    EXPECT_TRUE(model::solve(matrix));
}

TEST(Bundfuss2008ModelTest, RejectsInvalidMatrixShapes)
{
    matrix_integer empty;
    EXPECT_THROW(model::solve(empty), std::invalid_argument);

    matrix_integer non_square(2, 3);
    EXPECT_THROW(model::solve(non_square), std::invalid_argument);

    matrix_integer asymmetric;
    asymmetric.set_identity(2);
    asymmetric(0, 1) = integer(1);
    EXPECT_THROW(model::solve(asymmetric), std::invalid_argument);

}

TEST(Bundfuss2008ModelTest, DecidesOrdinaryCopositivity)
{
    constexpr auto ordinary = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), ordinary));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), ordinary));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), ordinary));
}

} // namespace
