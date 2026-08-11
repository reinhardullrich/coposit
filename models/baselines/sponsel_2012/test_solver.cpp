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

TEST(Sponsel2012ModelTest, HandlesMinimalStrictBoundary)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
}

TEST(Sponsel2012ModelTest, AcceptsStrictHCertificate)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {2, -1, 5, 2, 5, 2})));

    matrix_integer positive_definite_z_matrix(10, 10);
    for (size_t row = 0; row < 10; ++row) {
        for (size_t column = 0; column < 10; ++column) {
            positive_definite_z_matrix(row, column) = integer(row == column ? 10 : -1);
        }
    }
    EXPECT_TRUE(model::solve(positive_definite_z_matrix));
}

TEST(Sponsel2012ModelTest, SourceTraceConfirmsHCertificateAcceptance)
{
    baseline_source_trace::clear();
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {2, -1, 5, 2, 5, 2})));
    const std::vector<baseline_source_trace::event> expected{{"h-accept"}};
    EXPECT_EQ(baseline_source_trace::events, expected);
}

TEST(Sponsel2012ModelTest, RetainsFractionalBundfussSplit)
{
    // The selected edge has lambda = 5/12, exercising the inherited exact split when the H certificate does not apply.
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {2, -1, 1, 5, -2, 3})));
}

TEST(Sponsel2012ModelTest, FindsNegativeDirectionAfterPartitioning)
{
    matrix_integer matrix(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) matrix(row, column) = integer(row == column ? 5 : -2);
    }
    EXPECT_FALSE(model::solve(matrix));
}

TEST(Sponsel2012ModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer matrix = symmetric_matrix(3, {2, -1, 5, 2, 5, 2});
    fmpz_mat_scalar_mul_fmpz(matrix.native_handle(), matrix.native_handle(), scale.native_handle());
    EXPECT_TRUE(model::solve(matrix));
}

TEST(Sponsel2012ModelTest, HasNoFormerDimensionLimit)
{
    matrix_integer identity;
    identity.set_identity(70);
    EXPECT_TRUE(model::solve(identity));
}

TEST(Sponsel2012ModelTest, RejectsInvalidMatrixShapes)
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

TEST(Sponsel2012ModelTest, DecidesOrdinaryCopositivityWithThePublishedHCertificate)
{
    constexpr auto ordinary = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), ordinary));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), ordinary));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), ordinary));
}

} // namespace
