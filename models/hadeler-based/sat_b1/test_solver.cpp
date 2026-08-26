#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
bool sat_b1_prefers_negative_singular_orientation_for_testing(size_t positive_products,
                                                               size_t negative_products) noexcept;
size_t sat_b1_optimized_certificate_count_for_testing() noexcept;
size_t sat_b1_combined_ray_sweep_count_for_testing() noexcept;
size_t sat_b1_combined_ray_improvement_count_for_testing() noexcept;
size_t sat_b1_pair_curvature_exclusion_count_for_testing() noexcept;
size_t sat_b1_support_curvature_exclusion_count_for_testing() noexcept;
bool sat_b1_reduced_hessian_is_positive_definite_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices);
size_t sat_b1_shortlist_limit_for_testing(size_t matrix_dimension, size_t support_dimension);
bool sat_b1_prefers_ray_candidate_for_testing(size_t candidate_upper, size_t candidate_width, size_t candidate_gains,
                                               size_t candidate_losses, size_t current_upper, size_t current_width,
                                               size_t current_gains, size_t current_losses);
bool sat_b1_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool sat_b1_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper);
size_t sat_b1_fixed_support_upper_size_for_testing() noexcept;
size_t sat_b1_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t sat_b1_pair_exclusion_uncovered_count(
    size_t dimension, size_t cardinality, size_t first, size_t second);
size_t sat_b1_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
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

TEST(SatB1Test, PreservesExactCombinedClassifications)
{
    const auto strict = model::classify(symmetric_matrix(2, {2, -1, 2}));
    EXPECT_TRUE(strict.is_copositive);
    EXPECT_TRUE(strict.is_strictly_copositive);

    const auto boundary = model::classify(symmetric_matrix(2, {1, -1, 1}));
    EXPECT_TRUE(boundary.is_copositive);
    EXPECT_FALSE(boundary.is_strictly_copositive);

    const auto negative = model::classify(symmetric_matrix(2, {1, -2, 1}));
    EXPECT_FALSE(negative.is_copositive);
    EXPECT_FALSE(negative.is_strictly_copositive);
}

TEST(SatB1Test, RecognizesTheExactReducedHessianCases)
{
    matrix_integer identity;
    identity.set_identity(2);
    EXPECT_TRUE(model::sat_b1_reduced_hessian_is_positive_definite_for_testing(identity, {0, 1}));

    EXPECT_TRUE(model::sat_b1_reduced_hessian_is_positive_definite_for_testing(
        symmetric_matrix(2, {-1, -2, -1}), {0, 1}));
    EXPECT_FALSE(model::sat_b1_reduced_hessian_is_positive_definite_for_testing(
        symmetric_matrix(2, {1, 2, 1}), {0, 1}));
    EXPECT_TRUE(model::sat_b1_reduced_hessian_is_positive_definite_for_testing(
        symmetric_matrix(2, {0, 0, 1}), {0, 1}));
    EXPECT_FALSE(model::sat_b1_reduced_hessian_is_positive_definite_for_testing(
        symmetric_matrix(2, {1, 1, 1}), {0, 1}));

    matrix_integer zero(3, 3);
    EXPECT_FALSE(model::sat_b1_reduced_hessian_is_positive_definite_for_testing(zero, {0, 1, 2}));
}

TEST(SatB1Test, ReducedHessianCriterionMatchesExplicitTangentRestriction)
{
    fraction_free_ldlt_factorization factorization(2);
    for (slong a00 = -1; a00 <= 1; ++a00) {
        for (slong a01 = -1; a01 <= 1; ++a01) {
            for (slong a02 = -1; a02 <= 1; ++a02) {
                for (slong a11 = -1; a11 <= 1; ++a11) {
                    for (slong a12 = -1; a12 <= 1; ++a12) {
                        for (slong a22 = -1; a22 <= 1; ++a22) {
                            const matrix_integer matrix = symmetric_matrix(3, {a00, a01, a02, a11, a12, a22});
                            matrix_integer tangent = symmetric_matrix(
                                2, {a00 - 2 * a02 + a22, a01 - a02 - a12 + a22, a11 - 2 * a12 + a22});
                            const bool expected = factorization.factorize_inplace(tangent) != 0
                                && factorization.is_positive_definite();

                            SCOPED_TRACE(::testing::Message()
                                         << "A=[" << a00 << ',' << a01 << ',' << a02 << ';' << a11 << ',' << a12
                                         << ';' << a22 << ']');
                            EXPECT_EQ(model::sat_b1_reduced_hessian_is_positive_definite_for_testing(
                                          matrix, {0, 1, 2}),
                                      expected);
                        }
                    }
                }
            }
        }
    }
}

TEST(SatB1Test, ExhaustivelyClassifiesSmallTwoByTwoIntegerMatrices)
{
    for (slong diagonal_0 = -2; diagonal_0 <= 2; ++diagonal_0) {
        for (slong off_diagonal = -2; off_diagonal <= 2; ++off_diagonal) {
            for (slong diagonal_1 = -2; diagonal_1 <= 2; ++diagonal_1) {
                const auto classification = model::classify(
                    symmetric_matrix(2, {diagonal_0, off_diagonal, diagonal_1}));
                const slong determinant = diagonal_0 * diagonal_1 - off_diagonal * off_diagonal;
                const bool expected_copositive = diagonal_0 >= 0 && diagonal_1 >= 0
                    && (off_diagonal >= 0 || determinant >= 0);
                const bool expected_strict = diagonal_0 > 0 && diagonal_1 > 0
                    && (off_diagonal >= 0 || determinant > 0);

                SCOPED_TRACE(::testing::Message()
                             << "A=[" << diagonal_0 << ',' << off_diagonal << ';' << diagonal_1 << ']');
                EXPECT_EQ(classification.is_copositive, expected_copositive);
                EXPECT_EQ(classification.is_strictly_copositive, expected_strict);
            }
        }
    }
}

TEST(SatB1Test, PairCurvatureExclusionBlocksEverySuperset)
{
    const auto classification = model::classify(symmetric_matrix(2, {1, 2, 1}));
    EXPECT_TRUE(classification.is_copositive);
    EXPECT_TRUE(classification.is_strictly_copositive);
    EXPECT_EQ(model::sat_b1_pair_curvature_exclusion_count_for_testing(), 1U);
    EXPECT_EQ(model::sat_b1_pair_exclusion_uncovered_count(3, 1, 0, 1), 3U);
    EXPECT_EQ(model::sat_b1_pair_exclusion_uncovered_count(3, 2, 0, 1), 2U);
    EXPECT_EQ(model::sat_b1_pair_exclusion_uncovered_count(3, 3, 0, 1), 0U);
}

TEST(SatB1Test, RecordsExactUpwardOnlySupportHistory)
{
    std::ostringstream ignored_output;
    {
        diagnostics::reporter reporter(false, ignored_output, true, true);
        const auto classification = model::classify(symmetric_matrix(2, {1, 2, 1}));
        EXPECT_TRUE(classification.is_copositive);
        EXPECT_TRUE(classification.is_strictly_copositive);
    }

    const std::string history = diagnostics::detail::load_diagnostics();
    EXPECT_NE(history.find("event=certificate sequence=1 model=sat_b1 n=2 frontier=initial kind=pair_curvature "
                           "source=[1,2] coverage=upward lower=[1,2] upper=all exclude_empty=no "
                           "floating_checked=no exact_checked=yes"),
              std::string::npos)
        << history;
    EXPECT_EQ(history.find("coverage=downward"), std::string::npos) << history;
}

TEST(SatB1Test, PerSupportCurvatureExclusionSkipsTheRaySearch)
{
    support_context context(2);
    support lower = context.make();
    support upper = context.make();
    EXPECT_TRUE(model::sat_b1_certificate_for_testing(symmetric_matrix(2, {1, 2, 1}), {0, 1}, lower, upper));
    EXPECT_TRUE(context.contains(lower, 0));
    EXPECT_TRUE(context.contains(lower, 1));
    EXPECT_TRUE(context.contains(upper, 0));
    EXPECT_TRUE(context.contains(upper, 1));
    EXPECT_EQ(model::sat_b1_support_curvature_exclusion_count_for_testing(), 1U);
    EXPECT_EQ(model::sat_b1_optimized_certificate_count_for_testing(), 0U);
}

TEST(SatB1Test, SelectsAnExactImprovingHalfspaceDirection)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14})));
    EXPECT_GT(model::sat_b1_optimized_certificate_count_for_testing(), 0U);
}

TEST(SatB1Test, ChoosesTheSingularOrientationWithTheLargerUpperSet)
{
    EXPECT_TRUE(model::sat_b1_prefers_negative_singular_orientation_for_testing(1, 2));
    EXPECT_FALSE(model::sat_b1_prefers_negative_singular_orientation_for_testing(2, 1));
    EXPECT_FALSE(model::sat_b1_prefers_negative_singular_orientation_for_testing(2, 2));
}

TEST(SatB1Test, UsesWidthAsTheSecondaryObjective)
{
    const matrix_integer matrix = symmetric_matrix(2, {2, 1, 2});
    EXPECT_TRUE(model::sat_b1_check_support_for_testing(matrix, {0, 1}));
    EXPECT_EQ(model::sat_b1_fixed_support_upper_size_for_testing(), 2U);
    EXPECT_GT(model::sat_b1_optimized_certificate_count_for_testing(), 0U);
}

TEST(SatB1Test, ExposesTheExactOptimizedFixedSupportCertificate)
{
    matrix_integer identity;
    identity.set_identity(3);
    support_context context(3);
    support lower = context.make();
    support upper = context.make();

    EXPECT_TRUE(model::sat_b1_certificate_for_testing(identity, {0}, lower, upper));
    EXPECT_TRUE(context.contains(lower, 0));
    EXPECT_FALSE(context.contains(lower, 1));
    EXPECT_FALSE(context.contains(lower, 2));
    EXPECT_TRUE(context.contains(upper, 0));
    EXPECT_TRUE(context.contains(upper, 1));
    EXPECT_TRUE(context.contains(upper, 2));
}

TEST(SatB1Test, AdaptiveShortlistRemainsBounded)
{
    EXPECT_EQ(model::sat_b1_shortlist_limit_for_testing(5, 5), 5U);
    EXPECT_EQ(model::sat_b1_shortlist_limit_for_testing(80, 80), 27U);
    EXPECT_EQ(model::sat_b1_shortlist_limit_for_testing(3000, 3000), 64U);
    EXPECT_EQ(model::sat_b1_shortlist_limit_for_testing(3000, 10), 10U);
}

TEST(SatB1Test, RayRetentionNeverTradesWidthForIncidentalGains)
{
    EXPECT_FALSE(model::sat_b1_prefers_ray_candidate_for_testing(20, 4, 8, 0, 20, 5, 1, 0));
    EXPECT_TRUE(model::sat_b1_prefers_ray_candidate_for_testing(20, 5, 8, 0, 20, 5, 1, 0));
}

TEST(SatB1Test, CombinedRayEscapesACoordinateWiseUpperMaximum)
{
    matrix_integer matrix(6, 6);
    for (size_t index = 0; index < 3; ++index) matrix(index, index) = integer(1);
    for (size_t index = 3; index < 6; ++index) matrix(index, index) = integer(10);
    const slong outside[3][3]{{-2, 2, 1}, {2, -2, 1}, {1, 1, -3}};
    for (size_t row = 0; row < 3; ++row) {
        for (size_t column = 0; column < 3; ++column) {
            matrix(row + 3, column) = integer(outside[row][column]);
            matrix(column, row + 3) = matrix(row + 3, column);
        }
    }

    EXPECT_TRUE(model::sat_b1_check_support_for_testing(matrix, {0, 1, 2}));
    EXPECT_EQ(model::sat_b1_fixed_support_upper_size_for_testing(), 6U);
    EXPECT_EQ(model::sat_b1_combined_ray_sweep_count_for_testing(), 1U);
    EXPECT_EQ(model::sat_b1_combined_ray_improvement_count_for_testing(), 1U);
}

TEST(SatB1Test, SearchesAtMostTheBestTwoDistinctCombinedRays)
{
    matrix_integer matrix(10, 10);
    for (size_t index = 0; index < 4; ++index) matrix(index, index) = integer(1);
    for (size_t index = 4; index < 10; ++index) matrix(index, index) = integer(10);
    const slong outside[6][4]{
        {-2, 2, 1, 0}, {2, -2, 1, 0}, {1, 1, -3, 0}, {1, 0, -2, 2}, {1, 0, 2, -2}, {-3, 0, 1, 1}};
    for (size_t row = 0; row < 6; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            matrix(row + 4, column) = integer(outside[row][column]);
            matrix(column, row + 4) = matrix(row + 4, column);
        }
    }

    EXPECT_TRUE(model::sat_b1_check_support_for_testing(matrix, {0, 1, 2, 3}));
    EXPECT_EQ(model::sat_b1_combined_ray_sweep_count_for_testing(), 2U);
}

TEST(SatB1Test, StoresTheUnionOfDickinsonIntervalsInSat)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::sat_b1_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::sat_b1_uncovered_count(3, 2, intervals), 1U);
}

TEST(SatB1Test, AddsTheExistingCardinalityOutputOnlyToExpiringIntervals)
{
    EXPECT_EQ(model::sat_b1_interval_clause_size(8, 0b00000001, 0b00000111), 7U);
    EXPECT_EQ(model::sat_b1_interval_clause_size(8, 0b00000001, 0b11111111), 1U);
}

TEST(SatB1Test, PublishesTheChosenCertificateDistribution)
{
    matrix_integer identity;
    identity.set_identity(2);

    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_TRUE(model::solve(identity));
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_EQ(snapshot.certificate_cardinality_free_index_upper_size_counts,
              (std::map<std::tuple<size_t, size_t, size_t>, uint64_t>{{{1, 1, 2}, 2}}));
}

TEST(SatB1Test, RecordsFactoredSingularSupportsOnlyWhenDiagnosticsAreEnabled)
{
    const matrix_integer boundary = symmetric_matrix(2, {1, -1, 1});

    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_FALSE(model::solve(boundary));
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_EQ(snapshot.singular_cardinality_nullity_counts,
              (std::map<std::pair<size_t, size_t>, uint64_t>{{{2, 1}, 1}}));
}

TEST(SatB1Test, CooperativeTimeoutInterruptsSat)
{
    request_timeout();
    EXPECT_THROW(model::sat_b1_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
