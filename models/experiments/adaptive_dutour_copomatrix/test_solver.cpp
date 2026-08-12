#include <coposit/model.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <stdexcept>

using namespace coposit;

namespace coposit::model::adaptive_dutour_copomatrix_testing {
size_t streak_limit() noexcept;
bool solve_with_streak(const matrix_integer& matrix, size_t dutour_streak);
} // namespace coposit::model::adaptive_dutour_copomatrix_testing

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

TEST(AdaptiveDutourCopomatrixModelTest, UsesDirectStopsThroughOrderThree)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {3, -2, 3, 3, -2, 3})));
    EXPECT_FALSE(model::solve(symmetric_matrix(3, {1, -1, 0, 1, 0, 1})));
}

TEST(AdaptiveDutourCopomatrixModelTest, UsesNarrowCopomatrixReductions)
{
    matrix_integer identity;
    identity.set_identity(4);
    EXPECT_TRUE(model::solve(identity));

    EXPECT_TRUE(model::solve(symmetric_matrix(4, {4, 1, -1, 0, 4, 0, 0, 4, 0, 4})));
}

TEST(AdaptiveDutourCopomatrixModelTest, UsesDutourWhenEveryCopomatrixPivotIsWide)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {4, -1, 1, -1, 4, -1, 1, 4, -1, 4})));
}

TEST(AdaptiveDutourCopomatrixModelTest, ForcesCopomatrixAtTheHundredSplitCutoff)
{
    using namespace model::adaptive_dutour_copomatrix_testing;
    const matrix_integer wide = symmetric_matrix(4, {4, -1, 1, -1, 4, -1, 1, 4, -1, 4});

    EXPECT_EQ(streak_limit(), 100U);
    EXPECT_TRUE(solve_with_streak(wide, streak_limit()));
}

TEST(AdaptiveDutourCopomatrixModelTest, ClassifiesRetainedBoundaryRegression)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(5, {1, -1, 1, 2, -3, 2, -3, -3, 4, 5, 6, -4, 5, -8, 16})));
}

TEST(AdaptiveDutourCopomatrixModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer matrix = symmetric_matrix(4, {4, -1, 1, -1, 4, -1, 1, 4, -1, 4});
    fmpz_mat_scalar_mul_fmpz(matrix.native_handle(), matrix.native_handle(), scale.native_handle());
    EXPECT_TRUE(model::solve(matrix));
}

TEST(AdaptiveDutourCopomatrixModelTest, HasNoFormerDimensionLimit)
{
    matrix_integer identity;
    identity.set_identity(70);
    EXPECT_TRUE(model::solve(identity));
}

} // namespace
