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
size_t f2_pair_upward_count_for_testing() noexcept;
size_t f2_support_upward_count_for_testing() noexcept;
size_t f2_downward_count_for_testing() noexcept;
size_t f2_exact_count_for_testing() noexcept;
size_t f2_high_float_rejection_count_for_testing() noexcept;
size_t f2_optimized_certificate_count_for_testing() noexcept;
bool f2_floating_psd_candidate_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool f2_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool f2_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper);
std::pair<size_t, size_t> f2_storage_counts_for_testing();
std::pair<size_t, size_t> f2_interval_retirement_counts_for_testing();
bool f2_interval_scan_observes_timeout_for_testing();
size_t f2_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::vector<size_t>>& upward,
    const std::vector<std::vector<size_t>>& downward, const std::vector<std::vector<size_t>>& exact);
size_t f2_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t f2_high_filter_uncovered_count(
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

TEST(F2Test, AlternatesIndividualLowAndHighSupportsWithDirectionalPruning)
{
    f2_diagnostics::clear();
    const auto classification = model::classify(symmetric_matrix(3, {2, 0, 0, 2, 0, 2}));

    EXPECT_TRUE(classification.is_copositive);
    EXPECT_TRUE(classification.is_strictly_copositive);
    EXPECT_EQ(model::f2_pair_upward_count_for_testing(), 0U);
    EXPECT_EQ(model::f2_support_upward_count_for_testing(), 0U);
    EXPECT_EQ(model::f2_downward_count_for_testing(), 1U);
    EXPECT_EQ(model::f2_exact_count_for_testing(), 0U);
    EXPECT_NE(std::find(f2_diagnostics::events.begin(), f2_diagnostics::events.end(),
                       f2_diagnostics::event{"dickinson", 1}),
              f2_diagnostics::events.end());
}

TEST(F2Test, UsesTheFloatingFilterBeforeExactHighFrontierWork)
{
    f2_diagnostics::clear();
    (void)model::classify(symmetric_matrix(3, {1, -4, -2, 1, 0, 1}));

    const auto high = std::find(f2_diagnostics::events.begin(), f2_diagnostics::events.end(),
                                f2_diagnostics::event{"process_high", 3});
    ASSERT_NE(high, f2_diagnostics::events.end());
    ASSERT_NE(std::next(high), f2_diagnostics::events.end());
    EXPECT_EQ(*std::next(high), (f2_diagnostics::event{"high_float_reject", 3}));
    EXPECT_GT(model::f2_high_float_rejection_count_for_testing(), 0U);
}

TEST(F2Test, FloatingFilterOnlyProposesExactPositiveSemidefinitenessChecks)
{
    EXPECT_TRUE(model::f2_floating_psd_candidate_for_testing(
        symmetric_matrix(3, {2, 0, 0, 3, 0, 4}), {0, 1, 2}));
    EXPECT_TRUE(model::f2_floating_psd_candidate_for_testing(
        symmetric_matrix(3, {5, -1, 2, 5, 2, 2}), {0, 1, 2}));
    EXPECT_FALSE(model::f2_floating_psd_candidate_for_testing(
        symmetric_matrix(3, {1, 0, 0, -1, 0, 1}), {0, 1, 2}));
    EXPECT_FALSE(model::f2_floating_psd_candidate_for_testing(
        symmetric_matrix(2, {0, 1, 0}), {0, 1}));
}

TEST(F2Test, FloatingFilterUsesTheSelectedSubmatrixScale)
{
    const matrix_integer matrix = symmetric_matrix(3, {1'000'000'000'000'000'000L, 0, 0, 1, 0, 1});
    EXPECT_TRUE(model::f2_floating_psd_candidate_for_testing(matrix, {1, 2}));
}

TEST(F2Test, UsesConsistentAllOnesSystemToPruneSingularPsdSupportDownward)
{
    f2_diagnostics::clear();
    const auto classification = model::classify(symmetric_matrix(4, {3, -1, 1, 1, 3, 1, 1, 3, -1, 3}));

    EXPECT_TRUE(classification.is_copositive);
    EXPECT_TRUE(classification.is_strictly_copositive);
    EXPECT_EQ(model::f2_downward_count_for_testing(), 1U);
    EXPECT_NE(std::find(f2_diagnostics::events.begin(), f2_diagnostics::events.end(),
                       f2_diagnostics::event{"downward", 4}),
              f2_diagnostics::events.end());
}

TEST(F2Test, FloatingRejectionsRemainVisibleToTheExactLowFrontier)
{
    const std::vector<std::vector<size_t>> rejected{{0, 1}};
    EXPECT_EQ(model::f2_high_filter_uncovered_count(3, 2, rejected, true), 2U);
    EXPECT_EQ(model::f2_high_filter_uncovered_count(3, 2, rejected, false), 3U);
}

TEST(F2Test, BuildsAnExactDickinsonIntervalOnTheLowPath)
{
    matrix_integer identity;
    identity.set_identity(3);
    support lower(3);
    support upper(3);

    EXPECT_TRUE(model::f2_certificate_for_testing(identity, {0}, lower, upper));
    EXPECT_TRUE(lower.contains(0));
    EXPECT_FALSE(lower.contains(1));
    EXPECT_FALSE(lower.contains(2));
    EXPECT_TRUE(upper.contains(0));
    EXPECT_TRUE(upper.contains(1));
    EXPECT_TRUE(upper.contains(2));
}

TEST(F2Test, RetainsTheExactHalfspaceRayOptimization)
{
    const matrix_integer matrix = symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14});
    bool improved = false;
    for (uint64_t mask = 1; mask < 16 && !improved; ++mask) {
        std::vector<size_t> indices;
        for (size_t index = 0; index < 4; ++index)
            if ((mask & (uint64_t{1} << index)) != 0) indices.push_back(index);
        (void)model::f2_check_support_for_testing(matrix, indices);
        improved = model::f2_optimized_certificate_count_for_testing() > 0;
    }
    EXPECT_TRUE(improved);
}

TEST(F2Test, StoresDickinsonIntervalsInTheSeparateExactIndex)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::f2_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::f2_uncovered_count(3, 2, intervals), 1U);
}

TEST(F2Test, KeepsCurvatureRootsInTheGeneratorAndOtherCertificatesSeparate)
{
    const auto [generator_roots, extra_intervals] = model::f2_storage_counts_for_testing();
    EXPECT_EQ(generator_roots, 1U);
    EXPECT_EQ(extra_intervals, 2U);
}

TEST(F2Test, RetiresIntervalsOnlyAfterTheLowFrontierPassesTheirUpperCardinality)
{
    const auto [after_high_frontier, after_low_frontier] = model::f2_interval_retirement_counts_for_testing();
    EXPECT_EQ(after_high_frontier, 1U);
    EXPECT_EQ(after_low_frontier, 0U);
}

TEST(F2Test, LongIntervalScansObserveTimeouts)
{
    EXPECT_TRUE(model::f2_interval_scan_observes_timeout_for_testing());
}

TEST(F2Test, EncodesUpwardDownwardAndExactSupportBlocks)
{
    const std::vector<std::vector<size_t>> none;
    const std::vector<std::vector<size_t>> support_01{{0, 1}};

    EXPECT_EQ(model::f2_uncovered_count(3, 1, support_01, none, none), 3U);
    EXPECT_EQ(model::f2_uncovered_count(3, 2, support_01, none, none), 2U);
    EXPECT_EQ(model::f2_uncovered_count(3, 3, support_01, none, none), 0U);

    EXPECT_EQ(model::f2_uncovered_count(3, 1, none, support_01, none), 1U);
    EXPECT_EQ(model::f2_uncovered_count(3, 2, none, support_01, none), 2U);
    EXPECT_EQ(model::f2_uncovered_count(3, 3, none, support_01, none), 1U);

    EXPECT_EQ(model::f2_uncovered_count(3, 1, none, none, support_01), 3U);
    EXPECT_EQ(model::f2_uncovered_count(3, 2, none, none, support_01), 2U);
    EXPECT_EQ(model::f2_uncovered_count(3, 3, none, none, support_01), 1U);
}

TEST(F2Test, PreservesExactCombinedClassifications)
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

TEST(F2Test, ExhaustivelyClassifiesSmallTwoByTwoIntegerMatrices)
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

TEST(F2Test, MatchesTheIndependentExactThreeByThreeCriterion)
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

TEST(F2Test, AppliesPairCurvaturePrepass)
{
    const auto pair_excluded = model::classify(symmetric_matrix(2, {1, 2, 1}));
    EXPECT_TRUE(pair_excluded.is_copositive);
    EXPECT_TRUE(pair_excluded.is_strictly_copositive);
    EXPECT_EQ(model::f2_pair_upward_count_for_testing(), 1U);
}

} // namespace
