#include <coposit/model.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <stdexcept>

using namespace coposit;

namespace coposit::model {
bool frank_wolfe_witness_found_for_testing() noexcept;
bool frank_wolfe_line_step_used_for_testing() noexcept;
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

TEST(FrankWolfeSponselModelTest, RejectsWithExactCentreToVertexLineStep)
{
    // The centre has value 12/16 > 0. The minimum row sum is -7 at index 3, the exact line step is alpha=2/5, and the
    // homogeneous integer point (3,3,3,11) has quadratic value -100. Every diagonal and selected two-generator face passes.
    EXPECT_FALSE(model::solve(symmetric_matrix(4, {8, 6, -7, -3, 11, -2, -5, 15, -1, 2})));
    EXPECT_TRUE(model::frank_wolfe_line_step_used_for_testing());
    EXPECT_TRUE(model::frank_wolfe_witness_found_for_testing());
}

TEST(FrankWolfeSponselModelTest, RetainsSponselCertificatesAndSplits)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {2, -1, 5, 2, 5, 2})));
    EXPECT_FALSE(model::frank_wolfe_line_step_used_for_testing());
    EXPECT_FALSE(model::frank_wolfe_witness_found_for_testing());

    EXPECT_TRUE(model::solve(symmetric_matrix(3, {2, -1, 1, 5, -2, 3})));
}

TEST(FrankWolfeSponselModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer matrix = symmetric_matrix(4, {8, 6, -7, -3, 11, -2, -5, 15, -1, 2});
    fmpz_mat_scalar_mul_fmpz(matrix.native_handle(), matrix.native_handle(), scale.native_handle());
    EXPECT_FALSE(model::solve(matrix));
    EXPECT_TRUE(model::frank_wolfe_line_step_used_for_testing());
    EXPECT_TRUE(model::frank_wolfe_witness_found_for_testing());
}

TEST(FrankWolfeSponselModelTest, RetainsBoundaryAndDimensionBehavior)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));

    matrix_integer identity;
    identity.set_identity(70);
    EXPECT_TRUE(model::solve(identity));
}

} // namespace
