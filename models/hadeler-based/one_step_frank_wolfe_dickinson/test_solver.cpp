#include <coposit/model.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <stdexcept>

using namespace coposit;

namespace coposit::model {
bool one_step_frank_wolfe_witness_found_for_testing() noexcept;
bool one_step_frank_wolfe_line_used_for_testing() noexcept;
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

matrix_integer constant_off_diagonal(size_t dimension, slong diagonal, slong off_diagonal)
{
    matrix_integer matrix(dimension, dimension);
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = 0; column < dimension; ++column) {
            matrix(row, column) = integer(row == column ? diagonal : off_diagonal);
        }
    }
    return matrix;
}

TEST(OneStepFrankWolfeDickinsonModelTest, RejectsWithExactCentreToVertexLineStep)
{
    // T=12, the minimum row sum is -7 at j=3, alpha=2/5, and z=(3,3,3,11) has z^T A z=-100.
    EXPECT_FALSE(model::solve(symmetric_matrix(4, {8, 6, -7, -3, 11, -2, -5, 15, -1, 2})));
    EXPECT_TRUE(model::one_step_frank_wolfe_line_used_for_testing());
    EXPECT_TRUE(model::one_step_frank_wolfe_witness_found_for_testing());
}

TEST(OneStepFrankWolfeDickinsonModelTest, CombinedModeClassifiesTheBoundary)
{
    const auto result = model::classify(symmetric_matrix(4, {3, -1, -1, -1, 3, -1, -1, 3, -1, 3}));
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
}

TEST(OneStepFrankWolfeDickinsonModelTest, FallsBackToDickinsonAfterAPositiveLineMinimum)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(4, {1, -1, 0, 0, 1, 0, 0, 1, 0, 1})));
    EXPECT_TRUE(model::one_step_frank_wolfe_line_used_for_testing());
    EXPECT_FALSE(model::one_step_frank_wolfe_witness_found_for_testing());

    matrix_integer identity;
    identity.set_identity(4);
    EXPECT_TRUE(model::solve(identity));
    EXPECT_FALSE(model::one_step_frank_wolfe_line_used_for_testing());
}

TEST(OneStepFrankWolfeDickinsonModelTest, RejectsAnExactCentreWitness)
{
    EXPECT_FALSE(model::solve(constant_off_diagonal(4, 3, -1)));
    EXPECT_FALSE(model::one_step_frank_wolfe_line_used_for_testing());
    EXPECT_TRUE(model::one_step_frank_wolfe_witness_found_for_testing());
}

TEST(OneStepFrankWolfeDickinsonModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer matrix = symmetric_matrix(4, {8, 6, -7, -3, 11, -2, -5, 15, -1, 2});
    fmpz_mat_scalar_mul_fmpz(matrix.native_handle(), matrix.native_handle(), scale.native_handle());
    EXPECT_FALSE(model::solve(matrix));
    EXPECT_TRUE(model::one_step_frank_wolfe_line_used_for_testing());
    EXPECT_TRUE(model::one_step_frank_wolfe_witness_found_for_testing());
}

TEST(OneStepFrankWolfeDickinsonModelTest, RetainsLowDimensionAndUnboundedDimensionBehavior)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::one_step_frank_wolfe_witness_found_for_testing());

    EXPECT_FALSE(model::solve(constant_off_diagonal(70, 1, -1)));
    EXPECT_TRUE(model::one_step_frank_wolfe_witness_found_for_testing());
}

} // namespace
