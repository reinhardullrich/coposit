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


TEST(Hadeler1983ModelTest, RetainsStrictBoundaryRules)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {3, -2, 3, 3, -2, 3})));
}

TEST(Hadeler1983ModelTest, UsesOneRetainedSolveForNegativeDeterminants)
{
    EXPECT_FALSE(model::solve(constant_off_diagonal(4, 5, -2)));
    EXPECT_TRUE(model::solve(constant_off_diagonal(4, 1, 2)));
}

TEST(Hadeler1983ModelTest, SourceTracePreservesNumericMaskPairOrder)
{
    matrix_integer identity;
    identity.set_identity(4);
    baseline_source_trace::clear();
    EXPECT_TRUE(model::solve(identity));
    const std::vector<baseline_source_trace::event> expected{
        {"pair", 0, 1}, {"pair", 0, 2}, {"pair", 1, 2}, {"pair", 0, 3}, {"pair", 1, 3}, {"pair", 2, 3}};
    EXPECT_EQ(baseline_source_trace::events, expected);
}

TEST(Hadeler1983ModelTest, UsesOneNullspaceForSingularSubsets)
{
    EXPECT_FALSE(model::solve(constant_off_diagonal(4, 3, -1)));
    EXPECT_TRUE(model::solve(constant_off_diagonal(4, 1, 1)));

    constexpr slong vector[] = {1, -1, 1, -1};
    matrix_integer mixed_sign_nullspace(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            mixed_sign_nullspace(row, column) = integer((row == column ? 4 : 0) - vector[row] * vector[column]);
        }
    }
    EXPECT_TRUE(model::solve(mixed_sign_nullspace));
}

TEST(Hadeler1983ModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer negative_determinant = constant_off_diagonal(4, 5, -2);
    fmpz_mat_scalar_mul_fmpz(negative_determinant.native_handle(), negative_determinant.native_handle(), scale.native_handle());
    EXPECT_FALSE(model::solve(negative_determinant));

    matrix_integer singular = constant_off_diagonal(4, 3, -1);
    fmpz_mat_scalar_mul_fmpz(singular.native_handle(), singular.native_handle(), scale.native_handle());
    EXPECT_FALSE(model::solve(singular));
}

TEST(Hadeler1983ModelTest, ClassifiesBoundaryStressMatrix9161)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(5, {1, -1, 1, 2, -3, 2, -3, -3, 4, 5, 6, -4, 5, -8, 16})));
}

TEST(Hadeler1983ModelTest, HasNoFormerDimensionLimit)
{
    matrix_integer matrix;
    matrix.set_identity(70);
    matrix(69, 69).set_zero();
    EXPECT_FALSE(model::solve(matrix));
}

TEST(Hadeler1983ModelTest, RejectsInvalidMatrixShapes)
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

TEST(Hadeler1983ModelTest, DecidesOrdinaryCopositivity)
{
    constexpr auto ordinary = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), ordinary));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), ordinary));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), ordinary));
    EXPECT_TRUE(model::solve(constant_off_diagonal(4, 3, -1), ordinary));
    EXPECT_FALSE(model::solve(constant_off_diagonal(4, 2, -1), ordinary));
}

TEST(Hadeler1983ModelTest, ClassifiesBothPredicatesInOneTraversal)
{
    const model::copositivity_classification strict = model::classify(constant_off_diagonal(4, 5, -1));
    EXPECT_TRUE(strict.is_copositive);
    EXPECT_TRUE(strict.is_strictly_copositive);

    const model::copositivity_classification boundary = model::classify(constant_off_diagonal(4, 3, -1));
    EXPECT_TRUE(boundary.is_copositive);
    EXPECT_FALSE(boundary.is_strictly_copositive);

    const model::copositivity_classification negative = model::classify(constant_off_diagonal(4, 2, -1));
    EXPECT_FALSE(negative.is_copositive);
    EXPECT_FALSE(negative.is_strictly_copositive);
}

} // namespace
