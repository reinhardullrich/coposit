#include <coposit/model.hpp>
#include <coposit/progress.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <stdexcept>

using namespace coposit;

namespace coposit::model::adaptive_sponsel_copomatrix_testing {
size_t streak_limit() noexcept;
bool solve_with_streak(const matrix_integer& matrix, size_t sponsel_streak,
                       copositivity_mode mode = copositivity_mode::strictly_copositive);
size_t minimum_child_pivot(const matrix_integer& matrix);
} // namespace coposit::model::adaptive_sponsel_copomatrix_testing

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

TEST(AdaptiveSponselCopomatrixModelTest, UsesDirectStopsThroughOrderThree)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_TRUE(model::solve(symmetric_matrix(3, {3, -2, 3, 3, -2, 3})));
    EXPECT_FALSE(model::solve(symmetric_matrix(3, {1, -1, 0, 1, 0, 1})));
}

TEST(AdaptiveSponselCopomatrixModelTest, UsesCopositiveDirectBoundaries)
{
    constexpr auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
}

TEST(AdaptiveSponselCopomatrixModelTest, UsesNarrowCopomatrixReductions)
{
    matrix_integer identity;
    identity.set_identity(4);
    EXPECT_TRUE(model::solve(identity));

    EXPECT_TRUE(model::solve(symmetric_matrix(4, {4, 1, -1, 0, 4, 0, 0, 4, 0, 4})));
}

TEST(AdaptiveSponselCopomatrixModelTest, ChoosesTheFirstMinimumChildPivot)
{
    using model::adaptive_sponsel_copomatrix_testing::minimum_child_pivot;

    // Pivot 0 creates two children. Pivot 1 is the first pivot creating only the principal child.
    EXPECT_EQ(minimum_child_pivot(symmetric_matrix(4, {4, 1, -1, 0, 4, 0, 0, 4, -1, 4})), 1U);

    matrix_integer tied;
    tied.set_identity(4);
    EXPECT_EQ(minimum_child_pivot(tied), 0U);
}

TEST(AdaptiveSponselCopomatrixModelTest, HandlesZeroCopomatrixPivotsInCopositiveMode)
{
    constexpr auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {0, 1, 1, 1, 1, 0, 0, 1, 0, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(4, {0, -1, 1, 1, 1, 0, 0, 1, 0, 1}), copositive));
}

TEST(AdaptiveSponselCopomatrixModelTest, UsesSponselHCertificateWhenEveryPivotIsWide)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {4, -1, 1, -1, 4, -1, 1, 4, -1, 4})));
}

TEST(AdaptiveSponselCopomatrixModelTest, ReportsSponselAndCopomatrixWorkSeparately)
{
    progress::detail::reset();
    progress::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {4, -1, 1, -1, 4, -1, 1, 4, -1, 4})));
    const progress::snapshot sponsel = progress::detail::load();
    EXPECT_EQ(sponsel.kind, progress::metric::adaptive);
    EXPECT_GE(sponsel.sponsel_nodes, 1U);
    EXPECT_EQ(sponsel.copomatrix_nodes, 0U);

    matrix_integer identity;
    identity.set_identity(4);
    EXPECT_TRUE(model::solve(identity));
    const progress::snapshot copomatrix = progress::detail::load();
    progress::detail::state.enabled.store(false, std::memory_order_relaxed);
    progress::detail::reset();

    EXPECT_EQ(copomatrix.kind, progress::metric::adaptive);
    EXPECT_EQ(copomatrix.sponsel_nodes, 0U);
    EXPECT_GE(copomatrix.copomatrix_nodes, 1U);
    EXPECT_GE(copomatrix.copomatrix_children, 1U);
}

TEST(AdaptiveSponselCopomatrixModelTest, UsesCopositiveSponselRulesWhenEveryPivotIsWide)
{
    constexpr auto copositive = model::copositivity_mode::copositive;

    // zz^T for z=(1,1,-1,-1): every pivot is wide and every selected negative edge is an copositive boundary equality.
    const matrix_integer rank_one_boundary = symmetric_matrix(4, {1, 1, -1, -1, 1, -1, -1, 1, 1, 1});
    EXPECT_TRUE(model::solve(rank_one_boundary, copositive));
    EXPECT_FALSE(model::solve(rank_one_boundary));

    // Replacing positive entries by zero gives the positive-semidefinite Laplacian of the five-cycle.
    EXPECT_TRUE(model::solve(symmetric_matrix(5, {2, -1, 1, 1, -1, 2, -1, 1, 1, 2, -1, 1, 2, -1, 2}), copositive));
}

TEST(AdaptiveSponselCopomatrixModelTest, RetainsTheExactSponselSplit)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {5, -3, 2, -3, 5, -3, 2, 5, -3, 5})));
}

TEST(AdaptiveSponselCopomatrixModelTest, ForcesCopomatrixAtTheThousandSplitCutoff)
{
    using namespace model::adaptive_sponsel_copomatrix_testing;
    const matrix_integer wide = symmetric_matrix(4, {4, -1, 1, -1, 4, -1, 1, 4, -1, 4});

    EXPECT_EQ(streak_limit(), 1000U);
    EXPECT_TRUE(solve_with_streak(wide, streak_limit()));
}

TEST(AdaptiveSponselCopomatrixModelTest, ClassifiesRetainedBoundaryRegression)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(5, {1, -1, 1, 2, -3, 2, -3, -3, 4, 5, 6, -4, 5, -8, 16})));
}

TEST(AdaptiveSponselCopomatrixModelTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer matrix = symmetric_matrix(4, {5, -3, 2, -3, 5, -3, 2, 5, -3, 5});
    fmpz_mat_scalar_mul_fmpz(matrix.native_handle(), matrix.native_handle(), scale.native_handle());
    EXPECT_TRUE(model::solve(matrix));
}

TEST(AdaptiveSponselCopomatrixModelTest, HasNoFormerDimensionLimit)
{
    matrix_integer identity;
    identity.set_identity(70);
    EXPECT_TRUE(model::solve(identity));
}

} // namespace
