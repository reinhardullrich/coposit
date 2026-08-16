#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <tuple>
#include <vector>

using namespace coposit;

namespace coposit::model {
matrix_integer select_nullspace_vector_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices);
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

matrix_integer embedded_product(const matrix_integer& matrix, const std::vector<size_t>& indices, const matrix_integer& vector)
{
    matrix_integer product(matrix.rows(), 1);
    for (size_t row = 0; row < matrix.rows(); ++row) {
        for (size_t local = 0; local < indices.size(); ++local) {
            product(row, 0).addmul(matrix(row, indices[local]), vector(local, 0));
        }
    }
    return product;
}

unsigned long long choose(size_t n, size_t k)
{
    if (k > n) return 0;
    k = std::min(k, n - k);
    unsigned long long result = 1;
    for (size_t i = 1; i <= k; ++i) result = result * (n - k + i) / i;
    return result;
}

std::tuple<unsigned long long, size_t, size_t> score(
    const matrix_integer& matrix, const std::vector<size_t>& indices, const matrix_integer& vector)
{
    size_t lower_size = 0;
    for (size_t row = 0; row < vector.rows(); ++row) lower_size += !vector(row, 0).is_zero();

    const matrix_integer product = embedded_product(matrix, indices, vector);
    size_t upper_size = 0;
    for (size_t row = 0; row < product.rows(); ++row) upper_size += product(row, 0).sign() >= 0;

    unsigned long long future_supports = 0;
    for (size_t support_size = indices.size() + 1; support_size <= upper_size; ++support_size) {
        future_supports += choose(upper_size - lower_size, support_size - lower_size);
    }
    return {future_supports, upper_size - lower_size, upper_size};
}

matrix_integer signed_column(const matrix_integer& matrix, size_t column, int orientation)
{
    matrix_integer result(matrix.rows(), 1);
    for (size_t row = 0; row < matrix.rows(); ++row) {
        result(row, 0) = matrix(row, column);
        if (orientation < 0) result(row, 0).negate();
    }
    return result;
}

bool equal_vectors(const matrix_integer& left, const matrix_integer& right)
{
    for (size_t row = 0; row < left.rows(); ++row) {
        if (left(row, 0).compare(right(row, 0)) != 0) return false;
    }
    return true;
}

matrix_integer all_ones_principal_with_outside_rows(size_t principal_dimension, const std::vector<std::vector<slong>>& outside_rows)
{
    const size_t dimension = principal_dimension + outside_rows.size();
    matrix_integer matrix(dimension, dimension);
    for (size_t row = 0; row < principal_dimension; ++row) {
        for (size_t column = 0; column < principal_dimension; ++column) matrix(row, column).set_one();
    }
    for (size_t outside = 0; outside < outside_rows.size(); ++outside) {
        const size_t row = principal_dimension + outside;
        matrix(row, row).set_one();
        for (size_t column = 0; column < principal_dimension; ++column) {
            matrix(row, column) = integer(outside_rows[outside][column]);
            matrix(column, row) = matrix(row, column);
        }
    }
    return matrix;
}

TEST(NullitySupportPrunedDickinsonModelTest, DistinguishesStrictMatricesZerosAndNegativeWitnesses)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(NullitySupportPrunedDickinsonModelTest, CombinedModeClassifiesTheBoundary)
{
    const auto result = model::classify(constant_off_diagonal(4, 3, -1));
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
}

TEST(NullitySupportPrunedDickinsonModelTest, ChoosesTheBetterSignForNullityOne)
{
    const matrix_integer matrix = all_ones_principal_with_outside_rows(2, {{1, 0}});
    const std::vector<size_t> indices = {0, 1};
    const matrix_integer selected = model::select_nullspace_vector_for_test(matrix, indices);
    const matrix_integer product = embedded_product(matrix, indices, selected);

    EXPECT_EQ(product(0, 0).sign(), 0);
    EXPECT_EQ(product(1, 0).sign(), 0);
    EXPECT_GT(product(2, 0).sign(), 0);
}

TEST(NullitySupportPrunedDickinsonModelTest, FindsTheExactBestSignatureForNullityTwo)
{
    const matrix_integer matrix = all_ones_principal_with_outside_rows(3, {{1, 0, 0}, {0, 1, -1}});
    const std::vector<size_t> indices = {0, 1, 2};
    const matrix_integer selected = model::select_nullspace_vector_for_test(matrix, indices);
    const matrix_integer product = embedded_product(matrix, indices, selected);

    size_t support_size = 0;
    for (size_t row = 0; row < selected.rows(); ++row) support_size += !selected(row, 0).is_zero();
    EXPECT_EQ(support_size, 2U);
    for (size_t row = 0; row < product.rows(); ++row) EXPECT_GE(product(row, 0).sign(), 0);
    EXPECT_EQ(score(matrix, indices, selected), std::make_tuple(4ULL, 3U, 5U));
}

TEST(NullitySupportPrunedDickinsonModelTest, ChoosesTheBestSignedBasisVectorAboveNullityTwo)
{
    const matrix_integer matrix = all_ones_principal_with_outside_rows(4, {{1, 0, 0, 0}, {0, 1, -1, 0}, {0, 0, 1, -1}});
    const std::vector<size_t> indices = {0, 1, 2, 3};
    const matrix_integer selected = model::select_nullspace_vector_for_test(matrix, indices);

    matrix_integer principal(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) principal(row, column) = matrix(row, column);
    }
    matrix_integer factored(principal);
    fraction_free_ldlt_factorization factorization(4);
    ASSERT_EQ(factorization.factorize_inplace(factored), 0);
    ASSERT_EQ(factorization.rank(), 1U);
    matrix_integer basis(4, 3);
    factorization.nullspace_basis(basis, factored);

    auto expected = score(matrix, indices, signed_column(basis, 0, 1));
    bool selected_is_basis_vector = false;
    for (size_t column = 0; column < basis.cols(); ++column) {
        for (const int orientation : {1, -1}) {
            const matrix_integer candidate = signed_column(basis, column, orientation);
            expected = std::max(expected, score(matrix, indices, candidate));
            selected_is_basis_vector |= equal_vectors(selected, candidate);
        }
    }
    EXPECT_TRUE(selected_is_basis_vector);
    EXPECT_EQ(score(matrix, indices, selected), expected);
}

TEST(NullitySupportPrunedDickinsonModelTest, PrunesAllStrictSupersetsOfGlobalCertificates)
{
    matrix_integer identity;
    identity.set_identity(65);
    EXPECT_TRUE(model::solve(identity));
}

TEST(NullitySupportPrunedDickinsonModelTest, DoesNotPromoteBoundedCoverageToGlobalPruning)
{
    EXPECT_FALSE(model::solve(constant_off_diagonal(4, 3, -1)));
    EXPECT_TRUE(model::solve(constant_off_diagonal(4, 5, -1)));
}

TEST(NullitySupportPrunedDickinsonModelTest, HandlesSingularCertificatesExactly)
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

TEST(NullitySupportPrunedDickinsonModelTest, PreservesArbitraryPrecisionScaling)
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

TEST(NullitySupportPrunedDickinsonModelTest, ClassifiesBoundaryStressMatrix9161)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(5, {1, -1, 1, 2, -3, 2, -3, -3, 4, 5, 6, -4, 5, -8, 16})));
}

TEST(NullitySupportPrunedDickinsonModelTest, UsesPackedCoverageBeyondOneWord)
{
    matrix_integer not_strictly_copositive;
    not_strictly_copositive.set_identity(65);
    not_strictly_copositive(63, 64) = integer(-2);
    not_strictly_copositive(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(not_strictly_copositive));
}

} // namespace
