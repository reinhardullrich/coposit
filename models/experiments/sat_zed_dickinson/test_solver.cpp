#include <coposit/model.hpp>
#include <coposit/progress.hpp>
#include <coposit/timeout.hpp>

#include "source_trace.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
size_t sat_zed_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t sat_zed_interval_count(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
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

TEST(SatZedDickinsonTest, PreservesStrictDickinsonDecisions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(SatZedDickinsonTest, DistinguishesOrdinaryFromStrictCopositivity)
{
    const auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
}

TEST(SatZedDickinsonTest, ClassifiesBothPredicatesInOneTraversal)
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

TEST(SatZedDickinsonTest, SubtractsTheUnionOfBoundedIntervals)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::sat_zed_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::sat_zed_uncovered_count(3, 2, intervals), 1U);
}

TEST(SatZedDickinsonTest, StoresOneBlockingClausePerInterval)
{
    EXPECT_EQ(model::sat_zed_interval_count(8, 0b001, 0b111), 1U);
}

TEST(SatZedDickinsonTest, RepresentsAWholeZedDownsetWithAnEmptyLowerEndpoint)
{
    const std::vector<std::pair<uint64_t, uint64_t>> downset{{0, 0b101}};
    EXPECT_EQ(model::sat_zed_uncovered_count(3, 1, downset), 1U);
    EXPECT_EQ(model::sat_zed_uncovered_count(3, 2, downset), 2U);
    EXPECT_EQ(model::sat_zed_uncovered_count(3, 3, downset), 1U);
}

TEST(SatZedDickinsonTest, MatchesBruteForceForEveryPairOfFourDimensionalIntervals)
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
                EXPECT_EQ(model::sat_zed_uncovered_count(dimension, cardinality, intervals),
                          brute_uncovered_count(dimension, cardinality, intervals));
        }
    }
}

TEST(SatZedDickinsonTest, PublishesCertificateFreeIndexAndUpperSizeDistributionOnlyWithProgress)
{
    matrix_integer identity;
    identity.set_identity(2);

    progress::detail::reset();
    progress::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_TRUE(model::solve(identity));
    const progress::snapshot snapshot = progress::detail::load();
    progress::detail::state.enabled.store(false, std::memory_order_relaxed);
    progress::detail::reset();

    EXPECT_EQ(snapshot.certificate_cardinality_free_index_upper_size_counts,
              (std::map<std::tuple<size_t, size_t, size_t>, uint64_t>{{{1, 1, 2}, 2}}));
}

TEST(SatZedDickinsonTest, KeepsPackedSupportsBeyondOneWord)
{
    matrix_integer matrix;
    matrix.set_identity(65);
    matrix(63, 64) = integer(-2);
    matrix(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(matrix));
}

TEST(SatZedDickinsonTest, CooperativeTimeoutInterruptsSatSolving)
{
    request_timeout();
    EXPECT_THROW(model::sat_zed_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
