#include <coposit/model.hpp>

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

TEST(FracessaModelTest, ClassifiesInteriorAndBoundaryMinimaExactly)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, 0, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(FracessaModelTest, UsesCandidatePayoffSignsWithoutSecondOrderFiltering)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, 2, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {-1, 0, -10})));

    // Every simplex point is a non-isolated global maximizer of Q=-A; singleton pruning must retain the common value.
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {1, 1, 1, 1, 1, 1})));
}

TEST(FracessaModelTest, PreservesArbitraryPrecisionScaling)
{
    matrix_integer matrix = symmetric_matrix(2, {1, -1, 1});
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);
    fmpz_mat_scalar_mul_fmpz(matrix.native_handle(), matrix.native_handle(), scale.native_handle());
    EXPECT_FALSE(model::solve(matrix));
}

TEST(FracessaModelTest, SupportsPackedMultipleWordDimensions)
{
    matrix_integer strictly_copositive(70, 70);
    for (size_t row = 0; row < 70; ++row) {
        for (size_t column = 0; column < 70; ++column) strictly_copositive(row, column).set_one();
    }
    EXPECT_TRUE(model::solve(strictly_copositive));

    matrix_integer not_strictly_copositive;
    not_strictly_copositive.set_identity(70);
    not_strictly_copositive(69, 69) = integer(-1);
    EXPECT_FALSE(model::solve(not_strictly_copositive));
}

} // namespace
