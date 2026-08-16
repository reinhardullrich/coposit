#include <coposit/model.hpp>

#include "../source_diagnostics.hpp"

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

TEST(Safi2021ModelTest, HandlesStrictBoundary)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
}

TEST(Safi2021ModelTest, ExecutesPublishedSlicingBranches)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {100, 90, -54, 100, -3, 100})));
    EXPECT_FALSE(model::solve(symmetric_matrix(4, {100, -72, -59, 100, 100, -60, -46, 100, -60, 100})));
    EXPECT_TRUE(model::solve(symmetric_matrix(5, {3, -2, 3, 3, -2, 3, -2, 3, 3, 3, -2, 3, 3, -2, 3})));
}

TEST(Safi2021ModelTest, SourceTracePreservesSliceBeforeChildCertification)
{
    baseline_source_diagnostics::clear();
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    const std::vector<baseline_source_diagnostics::event> expected{{"split"}, {"certified"}};
    EXPECT_EQ(baseline_source_diagnostics::events, expected);
}

TEST(Safi2021ModelTest, HasNoFormerDimensionLimit)
{
    matrix_integer matrix;
    matrix.set_identity(70);
    EXPECT_TRUE(model::solve(matrix));
}

TEST(Safi2021ModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer matrix = symmetric_matrix(3, {100, 90, -54, 100, -3, 100});
    fmpz_mat_scalar_mul_fmpz(matrix.native_handle(), matrix.native_handle(), scale.native_handle());
    EXPECT_TRUE(model::solve(matrix));
}

TEST(Safi2021ModelTest, DecidesCopositivity)
{
    constexpr auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {0, -1, 1}), copositive));
}

} // namespace
