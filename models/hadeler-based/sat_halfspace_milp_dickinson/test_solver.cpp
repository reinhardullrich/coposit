#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"
#include "tiny_maximum_halfspaces.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
size_t sat_halfspace_milp_improvement_count_for_testing() noexcept;
bool sat_halfspace_milp_prefers_negative_singular_orientation_for_testing(
    size_t positive_products, size_t negative_products) noexcept;
size_t sat_halfspace_milp_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t sat_halfspace_milp_interval_count(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
size_t sat_halfspace_milp_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
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

size_t brute_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    size_t result = 0;
    for (uint64_t candidate = 1; candidate < (uint64_t{1} << dimension); ++candidate) {
        uint64_t copy = candidate;
        size_t count = 0;
        while (copy != 0) {
            copy &= copy - 1;
            ++count;
        }
        if (count != cardinality) continue;

        bool covered = false;
        for (const auto& [lower, upper] : intervals) covered |= (lower & ~candidate) == 0 && (candidate & ~upper) == 0;
        result += !covered;
    }
    return result;
}

TEST(SatHalfspaceMilpDickinsonTest, PreservesStrictDickinsonDecisions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(SatHalfspaceMilpDickinsonTest, TinyMilpFindsTheBestPositiveSimplexPoint)
{
    const std::vector<std::vector<double>> rows{{1.0, -1.0}, {-1.0, 1.0}, {1.0, 1.0}};
    model::detail::tiny_maximum_halfspaces optimizer(
        rows, 1e-7, 1, 10000, std::chrono::steady_clock::now() + std::chrono::seconds(1));
    const auto result = optimizer.solve();
    EXPECT_TRUE(result.optimal);
    EXPECT_EQ(result.satisfied, 3U);
    ASSERT_EQ(result.point.size(), 2U);
}

TEST(SatHalfspaceMilpDickinsonTest, UsesAnExactlyVerifiedMilpImprovement)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14})));
    EXPECT_GT(model::sat_halfspace_milp_improvement_count_for_testing(), 0U);
}

TEST(SatHalfspaceMilpDickinsonTest, ChoosesTheSingularOrientationWithTheLargerUpperSet)
{
    EXPECT_TRUE(model::sat_halfspace_milp_prefers_negative_singular_orientation_for_testing(1, 2));
    EXPECT_FALSE(model::sat_halfspace_milp_prefers_negative_singular_orientation_for_testing(2, 1));
    EXPECT_FALSE(model::sat_halfspace_milp_prefers_negative_singular_orientation_for_testing(2, 2));
}

TEST(SatHalfspaceMilpDickinsonTest, DistinguishesOrdinaryFromStrictCopositivity)
{
    const auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
}

TEST(SatHalfspaceMilpDickinsonTest, ClassifiesBothPredicatesInOneTraversal)
{
    const auto strict = model::classify(symmetric_matrix(1, {1}));
    EXPECT_TRUE(strict.is_copositive);
    EXPECT_TRUE(strict.is_strictly_copositive);

    const auto boundary = model::classify(symmetric_matrix(2, {1, -1, 1}));
    EXPECT_TRUE(boundary.is_copositive);
    EXPECT_FALSE(boundary.is_strictly_copositive);

    const auto negative = model::classify(symmetric_matrix(2, {1, -2, 1}));
    EXPECT_FALSE(negative.is_copositive);
    EXPECT_FALSE(negative.is_strictly_copositive);
}

TEST(SatHalfspaceMilpDickinsonTest, SubtractsTheUnionOfBoundedIntervals)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::sat_halfspace_milp_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::sat_halfspace_milp_uncovered_count(3, 2, intervals), 1U);
}

TEST(SatHalfspaceMilpDickinsonTest, StoresOneBlockingClausePerInterval)
{
    EXPECT_EQ(model::sat_halfspace_milp_interval_count(8, 0b001, 0b111), 1U);
}

TEST(SatHalfspaceMilpDickinsonTest, AddsTheExistingCardinalityOutputOnlyToExpiringIntervals)
{
    EXPECT_EQ(model::sat_halfspace_milp_interval_clause_size(8, 0b00000001, 0b00000111), 7U);
    EXPECT_EQ(model::sat_halfspace_milp_interval_clause_size(8, 0b00000001, 0b11111111), 1U);
}

TEST(SatHalfspaceMilpDickinsonTest, RepresentsAWholeDownsetWithAnEmptyLowerEndpoint)
{
    const std::vector<std::pair<uint64_t, uint64_t>> downset{{0, 0b101}};
    EXPECT_EQ(model::sat_halfspace_milp_uncovered_count(3, 1, downset), 1U);
    EXPECT_EQ(model::sat_halfspace_milp_uncovered_count(3, 2, downset), 2U);
    EXPECT_EQ(model::sat_halfspace_milp_uncovered_count(3, 3, downset), 1U);
}

TEST(SatHalfspaceMilpDickinsonTest, MatchesBruteForceForEveryPairOfFourDimensionalIntervals)
{
    constexpr size_t dimension = 4;
    std::vector<std::pair<uint64_t, uint64_t>> valid_intervals;
    for (uint64_t lower = 1; lower < (uint64_t{1} << dimension); ++lower)
        for (uint64_t upper = 0; upper < (uint64_t{1} << dimension); ++upper)
            if ((lower & ~upper) == 0) valid_intervals.emplace_back(lower, upper);

    for (const auto& first : valid_intervals) {
        for (const auto& second : valid_intervals) {
            const std::vector<std::pair<uint64_t, uint64_t>> intervals{first, second};
            for (size_t cardinality = 1; cardinality <= dimension; ++cardinality)
                EXPECT_EQ(model::sat_halfspace_milp_uncovered_count(dimension, cardinality, intervals),
                          brute_uncovered_count(dimension, cardinality, intervals));
        }
    }
}

TEST(SatHalfspaceMilpDickinsonTest, PublishesCertificateFreeIndexAndUpperSizeDistributionOnlyWithDiagnostics)
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

TEST(SatHalfspaceMilpDickinsonTest, RecordsFactoredSingularSupportsOnlyWhenDiagnosticsAreEnabled)
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

TEST(SatHalfspaceMilpDickinsonTest, KeepsPackedSupportsBeyondOneWord)
{
    matrix_integer matrix;
    matrix.set_identity(65);
    matrix(63, 64) = integer(-2);
    matrix(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(matrix));
}

TEST(SatHalfspaceMilpDickinsonTest, CooperativeTimeoutInterruptsSatSolving)
{
    request_timeout();
    EXPECT_THROW(model::sat_halfspace_milp_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
