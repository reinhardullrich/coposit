#include <coposit/fraction_free_ldlt.hpp>

#include <gtest/gtest.h>

#include <initializer_list>
#include <utility>

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

void expect_one_nullspace_vector(const matrix_integer& matrix, size_t expected_rank)
{
    matrix_integer factored(matrix);
    fraction_free_ldlt_factorization factorization(matrix.rows());
    ASSERT_EQ(factorization.factorize_inplace(factored), 0);
    ASSERT_EQ(factorization.rank(), expected_rank);

    matrix_integer vector(matrix.rows(), 1);
    factorization.one_nullspace_vector(vector, factored);

    matrix_integer product(matrix.rows(), 1);
    fmpz_mat_mul(product.native_handle(), matrix.native_handle(), vector.native_handle());
    EXPECT_TRUE(fmpz_mat_is_zero(product.native_handle()));
    EXPECT_FALSE(fmpz_mat_is_zero(vector.native_handle()));
}

void expect_nullspace_basis(const matrix_integer& matrix, size_t expected_rank)
{
    matrix_integer factored(matrix);
    fraction_free_ldlt_factorization factorization(matrix.rows());
    ASSERT_EQ(factorization.factorize_inplace(factored), 0);
    ASSERT_EQ(factorization.rank(), expected_rank);

    const size_t nullity = matrix.rows() - expected_rank;
    matrix_integer basis(matrix.rows(), nullity);
    factorization.nullspace_basis(basis, factored);

    matrix_integer product(matrix.rows(), nullity);
    fmpz_mat_mul(product.native_handle(), matrix.native_handle(), basis.native_handle());
    EXPECT_TRUE(fmpz_mat_is_zero(product.native_handle()));
    EXPECT_EQ(static_cast<size_t>(fmpz_mat_rank(basis.native_handle())), nullity);
}

void expect_consistent_solution(const matrix_integer& matrix, matrix_integer right_hand_side)
{
    matrix_integer factored(matrix);
    fraction_free_ldlt_factorization factorization(matrix.rows());
    ASSERT_EQ(factorization.factorize_inplace(factored), 0);

    matrix_integer expected(right_hand_side);

    integer denominator;
    ASSERT_TRUE(factorization.solve_consistent_inplace(right_hand_side, denominator, factored));
    ASSERT_GT(denominator.sign(), 0);

    matrix_integer product(matrix.rows(), right_hand_side.cols());
    fmpz_mat_mul(product.native_handle(), matrix.native_handle(), right_hand_side.native_handle());
    for (size_t row = 0; row < matrix.rows(); ++row) {
        for (size_t column = 0; column < right_hand_side.cols(); ++column) {
            integer scaled(expected(row, column));
            fmpz_mul(scaled.native_handle(), scaled.native_handle(), denominator.native_handle());
            EXPECT_EQ(product(row, column).compare(scaled), 0);
        }
    }
}

void expect_consistent_solution(const matrix_integer& matrix, std::initializer_list<slong> right_hand_side)
{
    matrix_integer values(matrix.rows(), 1);
    size_t row = 0;
    for (const slong value : right_hand_side) values(row++, 0) = integer(value);
    expect_consistent_solution(matrix, std::move(values));
}

TEST(FractionFreeLdltTest, RecoversOneVectorForEveryPositiveNullity)
{
    expect_one_nullspace_vector(symmetric_matrix(3, {0, 0, 0, 0, 0, 0}), 0);
    expect_one_nullspace_vector(symmetric_matrix(3, {0, 0, 0, 5, 0, 0}), 1); // Forces a symmetric swap.
    expect_one_nullspace_vector(symmetric_matrix(3, {0, 2, 0, 0, 3, 0}), 2); // Forces an addition and a nontrivial kernel vector.
    expect_one_nullspace_vector(symmetric_matrix(3, {2, 1, 3, 2, 3, 6}), 2);
    expect_one_nullspace_vector(
        symmetric_matrix(5, {22, 4, 10, 12, -4, 5, -3, 7, 6, 2, 8, -11, 4, 7, 11}), 3);
}

TEST(FractionFreeLdltTest, RecoversDenseKernelsAcrossRanks)
{
    for (size_t dimension = 2; dimension <= 12; ++dimension) {
        for (size_t rank = 1; rank < dimension; ++rank) {
            matrix_integer matrix(dimension, dimension);
            for (size_t row = 0; row < dimension; ++row) {
                for (size_t column = 0; column < dimension; ++column) {
                    slong value = 0;
                    for (size_t component = 0; component < rank; ++component) {
                        const slong left = row < rank ? static_cast<slong>(row == component)
                                                     : static_cast<slong>(1 + (component + 1) * (row - rank + 1));
                        const slong right = column < rank ? static_cast<slong>(column == component)
                                                         : static_cast<slong>(1 + (component + 1) * (column - rank + 1));
                        value += left * right;
                    }
                    matrix(row, column) = integer(value);
                }
            }
            expect_one_nullspace_vector(matrix, rank);
        }
    }
}

TEST(FractionFreeLdltTest, RecoversACompleteBasisAcrossRanks)
{
    for (size_t dimension = 2; dimension <= 8; ++dimension) {
        for (size_t rank = 0; rank < dimension; ++rank) {
            matrix_integer matrix(dimension, dimension);
            for (size_t row = 0; row < dimension; ++row) {
                for (size_t column = 0; column < dimension; ++column) {
                    slong value = 0;
                    for (size_t component = 0; component < rank; ++component) {
                        const slong left = row < rank ? static_cast<slong>(row == component)
                                                     : static_cast<slong>(1 + (component + 1) * (row - rank + 1));
                        const slong right = column < rank ? static_cast<slong>(column == component)
                                                         : static_cast<slong>(1 + (component + 1) * (column - rank + 1));
                        value += left * right;
                    }
                    matrix(row, column) = integer(value);
                }
            }
            expect_nullspace_basis(matrix, rank);
        }
    }
}

TEST(FractionFreeLdltTest, SolvesConsistentSingularSystemsFromTheRetainedFactorization)
{
    expect_consistent_solution(symmetric_matrix(2, {1, 1, 1}), {1, 1});
    expect_consistent_solution(symmetric_matrix(3, {0, 2, 0, 0, 3, 0}), {4, 11, 6}); // Forces an addition.

    matrix_integer zero(3, 3);
    expect_consistent_solution(zero, {0, 0, 0});

    matrix_integer multiple(2, 2);
    multiple(0, 0) = integer(1);
    multiple(1, 0) = integer(1);
    multiple(0, 1) = integer(2);
    multiple(1, 1) = integer(2);
    expect_consistent_solution(symmetric_matrix(2, {1, 1, 1}), std::move(multiple));
}

TEST(FractionFreeLdltTest, SolvesGeneratedConsistentSingularSystemsAcrossRanks)
{
    for (size_t dimension = 2; dimension <= 8; ++dimension) {
        for (size_t rank = 1; rank < dimension; ++rank) {
            matrix_integer matrix(dimension, dimension);
            for (size_t row = 0; row < dimension; ++row) {
                for (size_t column = 0; column < dimension; ++column) {
                    slong value = 0;
                    for (size_t component = 0; component < rank; ++component) {
                        const slong left = row < rank ? static_cast<slong>(row == component)
                                                     : static_cast<slong>(1 + (component + 1) * (row - rank + 1));
                        const slong right = column < rank ? static_cast<slong>(column == component)
                                                         : static_cast<slong>(1 + (component + 1) * (column - rank + 1));
                        value += left * right;
                    }
                    matrix(row, column) = integer(value);
                }
            }

            matrix_integer source(dimension, 1);
            for (size_t row = 0; row < dimension; ++row) source(row, 0) = integer(static_cast<slong>(row + 1));
            matrix_integer right_hand_side(dimension, 1);
            fmpz_mat_mul(right_hand_side.native_handle(), matrix.native_handle(), source.native_handle());
            expect_consistent_solution(matrix, std::move(right_hand_side));
        }
    }
}

TEST(FractionFreeLdltTest, SolvesASingularSystemAfterArbitraryPrecisionElimination)
{
    integer large;
    large.set_string("10000000000000000000000000000000000000001", 10);
    matrix_integer matrix(2, 2);
    matrix(0, 0).set_product(large, large);
    matrix(0, 1) = large;
    matrix(1, 0) = large;
    matrix(1, 1).set_one();

    matrix_integer source(2, 1);
    source(0, 0).set_one();
    source(1, 0) = integer(2);
    matrix_integer right_hand_side(2, 1);
    fmpz_mat_mul(right_hand_side.native_handle(), matrix.native_handle(), source.native_handle());
    expect_consistent_solution(matrix, std::move(right_hand_side));
}

TEST(FractionFreeLdltTest, RejectsAnInconsistentSingularSystem)
{
    matrix_integer factored = symmetric_matrix(2, {1, 1, 1});
    fraction_free_ldlt_factorization factorization(2);
    ASSERT_EQ(factorization.factorize_inplace(factored), 0);

    matrix_integer right_hand_side(2, 1);
    right_hand_side(0, 0) = integer(1);
    right_hand_side(1, 0) = integer(2);
    integer denominator;
    EXPECT_FALSE(factorization.solve_consistent_inplace(right_hand_side, denominator, factored));
}

TEST(FractionFreeLdltTest, DistinguishesPositiveSemidefiniteFromIndefiniteSingularMatrices)
{
    matrix_integer positive_semidefinite = symmetric_matrix(2, {1, -1, 1});
    fraction_free_ldlt_factorization factorization(2);
    EXPECT_EQ(factorization.factorize_inplace(positive_semidefinite), 0);
    EXPECT_TRUE(factorization.is_positive_semidefinite());
    EXPECT_FALSE(factorization.is_positive_definite());

    matrix_integer indefinite = symmetric_matrix(2, {0, 1, 0});
    EXPECT_EQ(factorization.factorize_inplace(indefinite), 1);
    EXPECT_FALSE(factorization.is_positive_semidefinite());
}

TEST(FractionFreeLdltTest, PublishesExactPreprocessingPivotDiagnosticsWhenRequested)
{
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    diagnostics::preprocessing_stage(diagnostics::preprocessing_phase::exact_factorization, 3, 0, 3);

    matrix_integer matrix = symmetric_matrix(3, {2, 1, 0, 2, 1, 2});
    fraction_free_ldlt_factorization factorization(3);
    EXPECT_EQ(factorization.factorize_inplace(matrix, true), 1);
    const diagnostics::snapshot value = diagnostics::detail::load();

    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();
    EXPECT_EQ(value.phase, diagnostics::preprocessing_phase::exact_factorization);
    EXPECT_EQ(value.current, 3U);
    EXPECT_EQ(value.maximum, 3U);
}

} // namespace
