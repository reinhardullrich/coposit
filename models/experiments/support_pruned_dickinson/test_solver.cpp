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

TEST(SupportPrunedDickinsonModelTest, DistinguishesStrictMatricesZerosAndNegativeWitnesses)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(SupportPrunedDickinsonModelTest, PrunesAllStrictSupersetsOfGlobalCertificates)
{
    matrix_integer identity;
    identity.set_identity(65);
    EXPECT_TRUE(model::solve(identity));
}

TEST(SupportPrunedDickinsonModelTest, DoesNotPromoteBoundedCoverageToGlobalPruning)
{
    EXPECT_FALSE(model::solve(constant_off_diagonal(4, 3, -1)));
    EXPECT_TRUE(model::solve(constant_off_diagonal(4, 5, -1)));
}

TEST(SupportPrunedDickinsonModelTest, HandlesSingularCertificatesExactly)
{
    constexpr slong vector[] = {1, -1, 1, -1};
    matrix_integer mixed_sign_nullspace(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            mixed_sign_nullspace(row, column) = integer((row == column ? 4 : 0) - vector[row] * vector[column]);
        }
    }
    EXPECT_TRUE(model::solve(mixed_sign_nullspace));
}

TEST(SupportPrunedDickinsonModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer strict = constant_off_diagonal(4, 5, -1);
    fmpz_mat_scalar_mul_fmpz(strict.native_handle(), strict.native_handle(), scale.native_handle());
    EXPECT_TRUE(model::solve(strict));

    matrix_integer boundary = constant_off_diagonal(4, 3, -1);
    fmpz_mat_scalar_mul_fmpz(boundary.native_handle(), boundary.native_handle(), scale.native_handle());
    EXPECT_FALSE(model::solve(boundary));
}

TEST(SupportPrunedDickinsonModelTest, ClassifiesBoundaryStressMatrix9161)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(5, {1, -1, 1, 2, -3, 2, -3, -3, 4, 5, 6, -4, 5, -8, 16})));
}

TEST(SupportPrunedDickinsonModelTest, UsesPackedCoverageBeyondOneWord)
{
    matrix_integer not_strictly_copositive;
    not_strictly_copositive.set_identity(65);
    not_strictly_copositive(63, 64) = integer(-2);
    not_strictly_copositive(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(not_strictly_copositive));
}

} // namespace
