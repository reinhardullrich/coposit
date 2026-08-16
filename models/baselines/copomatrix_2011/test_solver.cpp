#include <coposit/model.hpp>

#include "../source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <cstddef>
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

TEST(Copomatrix2011ModelTest, AppliesStrictEqualityBoundaries)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(Copomatrix2011ModelTest, UsesPrincipalAndAllNegativeSchurChildren)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {2, -1, 0, 2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(3, {1, -1, 0, 1, 0, 1})));
}

TEST(Copomatrix2011ModelTest, SourceTracePreservesPrincipalBeforeSchur)
{
    baseline_source_diagnostics::clear();
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    const std::vector<baseline_source_diagnostics::event> expected{
        {"diagonal-scan", 2}, {"principal", 1}, {"schur", 1}};
    EXPECT_EQ(baseline_source_diagnostics::events, expected);
}

TEST(Copomatrix2011ModelTest, UsesXuYaoNegativeStaircase)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(
        6, {10, 1, 1, -1, -1, -1, 10, 0, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 10, 0, 10})));
    EXPECT_FALSE(model::solve(symmetric_matrix(4, {1, 1, -1, -1, 1, 0, 0, 1, 0, 1})));
}

TEST(Copomatrix2011ModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer matrix = symmetric_matrix(4, {1, 1, -1, -1, 1, 0, 0, 1, 0, 1});
    fmpz_mat_scalar_mul_fmpz(matrix.native_handle(), matrix.native_handle(), scale.native_handle());
    EXPECT_FALSE(model::solve(matrix));
}

TEST(Copomatrix2011ModelTest, HasNoFormerDimensionLimit)
{
    matrix_integer identity;
    identity.set_identity(70);
    EXPECT_TRUE(model::solve(identity));
}

TEST(Copomatrix2011ModelTest, DecidesCopositivity)
{
    constexpr auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {0, 1, 1, 1, 1, 0, 0, 1, 0, 1}), copositive));
}

} // namespace
