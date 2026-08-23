#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
size_t sat_c1_pair_upward_count_for_testing() noexcept;
size_t sat_c1_support_upward_count_for_testing() noexcept;
size_t sat_c1_downward_count_for_testing() noexcept;
size_t sat_c1_exact_count_for_testing() noexcept;
size_t sat_c1_high_float_rejection_count_for_testing() noexcept;
size_t sat_c1_optimized_certificate_count_for_testing() noexcept;
size_t sat_c1_local_curvature_search_count_for_testing() noexcept;
size_t sat_c1_local_curvature_core_count_for_testing() noexcept;
size_t sat_c1_local_curvature_exact_query_count_for_testing() noexcept;
bool sat_c1_floating_psd_candidate_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool sat_c1_reduced_hessian_is_positive_definite_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices);
bool sat_c1_schur_curvature_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& lower,
    const std::vector<size_t>& upper, const std::vector<size_t>& query);
std::vector<std::vector<size_t>> sat_c1_curvature_boundary_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& lower, const std::vector<size_t>& upper);
bool sat_c1_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool sat_c1_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper);
size_t sat_c1_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::vector<size_t>>& upward,
    const std::vector<std::vector<size_t>>& downward, const std::vector<std::vector<size_t>>& exact);
size_t sat_c1_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t sat_c1_high_filter_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::vector<size_t>>& rejected, bool high_frontier);
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

TEST(SatC1Test, AlternatesIndividualLowAndHighSupportsWithDirectionalPruning)
{
    sat_c1_diagnostics::clear();
    const auto classification = model::classify(symmetric_matrix(3, {2, 0, 0, 2, 0, 2}));

    EXPECT_TRUE(classification.is_copositive);
    EXPECT_TRUE(classification.is_strictly_copositive);
    EXPECT_EQ(model::sat_c1_pair_upward_count_for_testing(), 0U);
    EXPECT_EQ(model::sat_c1_support_upward_count_for_testing(), 0U);
    EXPECT_EQ(model::sat_c1_downward_count_for_testing(), 1U);
    EXPECT_EQ(model::sat_c1_exact_count_for_testing(), 0U);
    EXPECT_NE(std::find(sat_c1_diagnostics::events.begin(), sat_c1_diagnostics::events.end(),
                       sat_c1_diagnostics::event{"dickinson", 1}),
              sat_c1_diagnostics::events.end());
}

TEST(SatC1Test, UsesTheFloatingFilterBeforeExactHighFrontierWork)
{
    sat_c1_diagnostics::clear();
    (void)model::classify(symmetric_matrix(3, {1, -4, -2, 1, 0, 1}));

    const auto high = std::find(sat_c1_diagnostics::events.begin(), sat_c1_diagnostics::events.end(),
                                sat_c1_diagnostics::event{"process_high", 3});
    ASSERT_NE(high, sat_c1_diagnostics::events.end());
    ASSERT_NE(std::next(high), sat_c1_diagnostics::events.end());
    EXPECT_EQ(*std::next(high), (sat_c1_diagnostics::event{"high_float_reject", 3}));
    EXPECT_GT(model::sat_c1_high_float_rejection_count_for_testing(), 0U);
}

TEST(SatC1Test, FloatingFilterOnlyProposesExactPositiveSemidefinitenessChecks)
{
    EXPECT_TRUE(model::sat_c1_floating_psd_candidate_for_testing(
        symmetric_matrix(3, {2, 0, 0, 3, 0, 4}), {0, 1, 2}));
    EXPECT_TRUE(model::sat_c1_floating_psd_candidate_for_testing(
        symmetric_matrix(3, {5, -1, 2, 5, 2, 2}), {0, 1, 2}));
    EXPECT_FALSE(model::sat_c1_floating_psd_candidate_for_testing(
        symmetric_matrix(3, {1, 0, 0, -1, 0, 1}), {0, 1, 2}));
    EXPECT_FALSE(model::sat_c1_floating_psd_candidate_for_testing(
        symmetric_matrix(2, {0, 1, 0}), {0, 1}));
}

TEST(SatC1Test, FloatingFilterUsesTheSelectedSubmatrixScale)
{
    const matrix_integer matrix = symmetric_matrix(3, {1'000'000'000'000'000'000L, 0, 0, 1, 0, 1});
    EXPECT_TRUE(model::sat_c1_floating_psd_candidate_for_testing(matrix, {1, 2}));
}

TEST(SatC1Test, UsesConsistentAllOnesSystemToPruneSingularPsdSupportDownward)
{
    sat_c1_diagnostics::clear();
    const auto classification = model::classify(symmetric_matrix(4, {3, -1, 1, 1, 3, 1, 1, 3, -1, 3}));

    EXPECT_TRUE(classification.is_copositive);
    EXPECT_TRUE(classification.is_strictly_copositive);
    EXPECT_EQ(model::sat_c1_downward_count_for_testing(), 1U);
    EXPECT_NE(std::find(sat_c1_diagnostics::events.begin(), sat_c1_diagnostics::events.end(),
                       sat_c1_diagnostics::event{"downward", 4}),
              sat_c1_diagnostics::events.end());
}

TEST(SatC1Test, FloatingRejectionsRemainVisibleToTheExactLowFrontier)
{
    const std::vector<std::vector<size_t>> rejected{{0, 1}};
    EXPECT_EQ(model::sat_c1_high_filter_uncovered_count(3, 2, rejected, true), 2U);
    EXPECT_EQ(model::sat_c1_high_filter_uncovered_count(3, 2, rejected, false), 3U);
}

TEST(SatC1Test, BuildsAnExactDickinsonIntervalOnTheLowPath)
{
    matrix_integer identity;
    identity.set_identity(3);
    support lower(3);
    support upper(3);

    EXPECT_TRUE(model::sat_c1_certificate_for_testing(identity, {0}, lower, upper));
    EXPECT_TRUE(lower.contains(0));
    EXPECT_FALSE(lower.contains(1));
    EXPECT_FALSE(lower.contains(2));
    EXPECT_TRUE(upper.contains(0));
    EXPECT_TRUE(upper.contains(1));
    EXPECT_TRUE(upper.contains(2));
}

TEST(SatC1Test, RetainsTheExactHalfspaceRayOptimization)
{
    const matrix_integer matrix = symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14});
    bool improved = false;
    for (uint64_t mask = 1; mask < 16 && !improved; ++mask) {
        std::vector<size_t> indices;
        for (size_t index = 0; index < 4; ++index)
            if ((mask & (uint64_t{1} << index)) != 0) indices.push_back(index);
        (void)model::sat_c1_check_support_for_testing(matrix, indices);
        improved = model::sat_c1_optimized_certificate_count_for_testing() > 0;
    }
    EXPECT_TRUE(improved);
}

TEST(SatC1Test, SchurResidualMatchesTheOriginalReducedHessian)
{
    const matrix_integer matrix = symmetric_matrix(4, {3, 0, 2, -1, 4, -2, 1, 1, 0, 2});
    const std::vector<size_t> lower{0, 1};
    const std::vector<size_t> upper{0, 1, 2, 3};
    for (uint64_t optional_mask = 0; optional_mask < 4; ++optional_mask) {
        std::vector<size_t> query = lower;
        if ((optional_mask & 1) != 0) query.push_back(2);
        if ((optional_mask & 2) != 0) query.push_back(3);
        EXPECT_EQ(model::sat_c1_schur_curvature_for_testing(matrix, lower, upper, query),
                  model::sat_c1_reduced_hessian_is_positive_definite_for_testing(matrix, query));
    }

    matrix_integer identity;
    identity.set_identity(5);
    const std::vector<size_t> larger_lower{0, 1, 2};
    const std::vector<size_t> larger_upper{0, 1, 2, 3, 4};
    for (uint64_t optional_mask = 0; optional_mask < 4; ++optional_mask) {
        std::vector<size_t> query = larger_lower;
        if ((optional_mask & 1) != 0) query.push_back(3);
        if ((optional_mask & 2) != 0) query.push_back(4);
        EXPECT_EQ(model::sat_c1_schur_curvature_for_testing(identity, larger_lower, larger_upper, query),
                  model::sat_c1_reduced_hessian_is_positive_definite_for_testing(identity, query));
    }
}

TEST(SatC1Test, SchurResidualMatchesNontrivialExactCrossBlocks)
{
    uint64_t state = 0x9e3779b97f4a7c15ULL;
    const auto next_entry = [&]() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<slong>((state >> 32) % 7) - 3;
    };

    const std::vector<size_t> lower{0, 1, 2};
    const std::vector<size_t> upper{0, 1, 2, 3, 4};
    for (size_t sample = 0; sample < 64; ++sample) {
        matrix_integer matrix(5, 5);
        for (size_t row = 0; row < 5; ++row) {
            for (size_t column = row; column < 5; ++column) {
                const slong value = row < 3 && column < 3 ? static_cast<slong>(row == column) : next_entry();
                matrix(row, column) = integer(value);
                matrix(column, row) = matrix(row, column);
            }
        }

        for (uint64_t optional_mask = 0; optional_mask < 4; ++optional_mask) {
            std::vector<size_t> query = lower;
            if ((optional_mask & 1) != 0) query.push_back(3);
            if ((optional_mask & 2) != 0) query.push_back(4);
            SCOPED_TRACE(::testing::Message() << "sample=" << sample << ", mask=" << optional_mask);
            EXPECT_EQ(model::sat_c1_schur_curvature_for_testing(matrix, lower, upper, query),
                      model::sat_c1_reduced_hessian_is_positive_definite_for_testing(matrix, query));
        }
    }
}

TEST(SatC1Test, FindsAHigherOrderCurvatureCoreInsideAnInterval)
{
    const matrix_integer matrix = symmetric_matrix(3, {1, -2, 0, 1, 0, 0});
    const auto cores = model::sat_c1_curvature_boundary_for_testing(matrix, {2}, {0, 1, 2});

    EXPECT_EQ(cores, (std::vector<std::vector<size_t>>{{0, 1, 2}}));
    EXPECT_EQ(model::sat_c1_local_curvature_search_count_for_testing(), 1U);
    EXPECT_EQ(model::sat_c1_local_curvature_core_count_for_testing(), 1U);
    EXPECT_GT(model::sat_c1_local_curvature_exact_query_count_for_testing(), 0U);
}

TEST(SatC1Test, ExactGoodUpperEndpointSkipsTheLocalSatSearch)
{
    const matrix_integer matrix = symmetric_matrix(
        3, {1'000'000'000'000'000'000L, 0, 0, 1, 0, 0});
    const auto cores = model::sat_c1_curvature_boundary_for_testing(matrix, {2}, {0, 1, 2});

    EXPECT_TRUE(cores.empty());
    EXPECT_EQ(model::sat_c1_local_curvature_search_count_for_testing(), 0U);
    EXPECT_EQ(model::sat_c1_local_curvature_exact_query_count_for_testing(), 1U);
}

TEST(SatC1Test, StoresDickinsonIntervalsInTheSharedSatState)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::sat_c1_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::sat_c1_uncovered_count(3, 2, intervals), 1U);
}

TEST(SatC1Test, EncodesUpwardDownwardAndExactSupportBlocks)
{
    const std::vector<std::vector<size_t>> none;
    const std::vector<std::vector<size_t>> support_01{{0, 1}};

    EXPECT_EQ(model::sat_c1_uncovered_count(3, 1, support_01, none, none), 3U);
    EXPECT_EQ(model::sat_c1_uncovered_count(3, 2, support_01, none, none), 2U);
    EXPECT_EQ(model::sat_c1_uncovered_count(3, 3, support_01, none, none), 0U);

    EXPECT_EQ(model::sat_c1_uncovered_count(3, 1, none, support_01, none), 1U);
    EXPECT_EQ(model::sat_c1_uncovered_count(3, 2, none, support_01, none), 2U);
    EXPECT_EQ(model::sat_c1_uncovered_count(3, 3, none, support_01, none), 1U);

    EXPECT_EQ(model::sat_c1_uncovered_count(3, 1, none, none, support_01), 3U);
    EXPECT_EQ(model::sat_c1_uncovered_count(3, 2, none, none, support_01), 2U);
    EXPECT_EQ(model::sat_c1_uncovered_count(3, 3, none, none, support_01), 1U);
}

TEST(SatC1Test, PreservesExactCombinedClassifications)
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

TEST(SatC1Test, ExhaustivelyClassifiesSmallTwoByTwoIntegerMatrices)
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

TEST(SatC1Test, MatchesTheIndependentExactThreeByThreeCriterion)
{
    for (slong a00 = -1; a00 <= 1; ++a00) {
        for (slong a01 = -1; a01 <= 1; ++a01) {
            for (slong a02 = -1; a02 <= 1; ++a02) {
                for (slong a11 = -1; a11 <= 1; ++a11) {
                    for (slong a12 = -1; a12 <= 1; ++a12) {
                        for (slong a22 = -1; a22 <= 1; ++a22) {
                            const matrix_integer matrix = symmetric_matrix(3, {a00, a01, a02, a11, a12, a22});
                            const auto expected = small_copositivity::classify(matrix);
                            const auto actual = model::classify(matrix);

                            SCOPED_TRACE(::testing::Message()
                                         << "A=[" << a00 << ',' << a01 << ',' << a02 << ';' << a11 << ',' << a12
                                         << ';' << a22 << ']');
                            EXPECT_EQ(actual.is_copositive, expected.is_copositive);
                            EXPECT_EQ(actual.is_strictly_copositive, expected.is_strictly_copositive);
                        }
                    }
                }
            }
        }
    }
}

TEST(SatC1Test, AppliesPairCurvaturePrepass)
{
    const auto pair_excluded = model::classify(symmetric_matrix(2, {1, 2, 1}));
    EXPECT_TRUE(pair_excluded.is_copositive);
    EXPECT_TRUE(pair_excluded.is_strictly_copositive);
    EXPECT_EQ(model::sat_c1_pair_upward_count_for_testing(), 1U);
}

} // namespace
