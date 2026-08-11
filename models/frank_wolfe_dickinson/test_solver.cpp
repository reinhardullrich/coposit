#include <coposit/model.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <stdexcept>

using namespace coposit;

namespace coposit::model {
bool frank_wolfe_witness_found_for_testing() noexcept;
bool iterative_frank_wolfe_witness_found_for_testing() noexcept;
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

TEST(FrankWolfeDickinsonModelTest, FindsAnExactHigherOrderNegativeWitness)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(5, {12, 3, 0, -7, -5, 8, 0, -1, 3, 12, -8, -2, 10, -1, 11})));
    EXPECT_TRUE(model::frank_wolfe_witness_found_for_testing());
    EXPECT_TRUE(model::iterative_frank_wolfe_witness_found_for_testing());
}

TEST(FrankWolfeDickinsonModelTest, FallsBackToDickinson)
{
    EXPECT_TRUE(model::solve(constant_off_diagonal(4, 1, 1)));
    EXPECT_FALSE(model::frank_wolfe_witness_found_for_testing());

    EXPECT_FALSE(model::solve(symmetric_matrix(4, {0, 1, 1, 1, 1, 1, 1, 1, 1, 1})));
    EXPECT_TRUE(model::frank_wolfe_witness_found_for_testing());
    EXPECT_FALSE(model::iterative_frank_wolfe_witness_found_for_testing());

    EXPECT_FALSE(model::solve(constant_off_diagonal(4, 3, -1)));
    EXPECT_TRUE(model::frank_wolfe_witness_found_for_testing());
    EXPECT_FALSE(model::iterative_frank_wolfe_witness_found_for_testing());
}

TEST(FrankWolfeDickinsonModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer negative = symmetric_matrix(5, {12, 3, 0, -7, -5, 8, 0, -1, 3, 12, -8, -2, 10, -1, 11});
    fmpz_mat_scalar_mul_fmpz(negative.native_handle(), negative.native_handle(), scale.native_handle());
    EXPECT_FALSE(model::solve(negative));
    EXPECT_TRUE(model::frank_wolfe_witness_found_for_testing());

    matrix_integer strict = constant_off_diagonal(4, 1, 1);
    fmpz_mat_scalar_mul_fmpz(strict.native_handle(), strict.native_handle(), scale.native_handle());
    EXPECT_TRUE(model::solve(strict));
}

TEST(FrankWolfeDickinsonModelTest, DistinguishesLowDimensionalCases)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(FrankWolfeDickinsonModelTest, RejectsInvalidMatrixShapes)
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

} // namespace
