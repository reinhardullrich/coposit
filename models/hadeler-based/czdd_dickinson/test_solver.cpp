#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
std::pair<size_t, size_t> czdd_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t czdd_maximum_interval_chain(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
size_t czdd_expired_bucket_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
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

TEST(CzddDickinsonTest, PreservesStrictDickinsonDecisions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(CzddDickinsonTest, ClassifiesCopositiveBoundary)
{
    const matrix_integer boundary = symmetric_matrix(4, {3, -1, -1, -1, 3, -1, -1, 3, -1, 3});
    const auto result = model::classify(boundary);
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
}

TEST(CzddDickinsonTest, SubtractsTheUnionOfBoundedIntervals)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::czdd_uncovered_count(3, 1, intervals).first, 1U);
    EXPECT_EQ(model::czdd_uncovered_count(3, 2, intervals).first, 1U);
}

TEST(CzddDickinsonTest, DeletesExpiredIntervalsWithoutDamagingOverlappingLiveCoverage)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b0001, 0b0011}, {0b0001, 0b1111}};
    EXPECT_EQ(model::czdd_expired_bucket_count(4, 3, intervals), 1U);
    EXPECT_EQ(model::czdd_uncovered_count(4, 3, intervals).first, brute_uncovered_count(4, 3, intervals));
}

TEST(CzddDickinsonTest, CompressesADontCareChain)
{
    EXPECT_EQ(model::czdd_maximum_interval_chain(8, 0, 0xff), 8U);
}

TEST(CzddDickinsonTest, RepresentsAWholeDownsetWithAnEmptyLowerEndpoint)
{
    const std::vector<std::pair<uint64_t, uint64_t>> downset{{0, 0b101}};
    EXPECT_EQ(model::czdd_uncovered_count(3, 1, downset).first, 1U);
    EXPECT_EQ(model::czdd_uncovered_count(3, 2, downset).first, 2U);
    EXPECT_EQ(model::czdd_uncovered_count(3, 3, downset).first, 1U);
}

TEST(CzddDickinsonTest, MatchesBruteForceForEveryPairOfFourDimensionalIntervals)
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
                EXPECT_EQ(model::czdd_uncovered_count(dimension, cardinality, intervals).first,
                          brute_uncovered_count(dimension, cardinality, intervals));
        }
    }
}

TEST(CzddDickinsonTest, PublishesCertificateFreeIndexAndUpperSizeDistributionOnlyWithDiagnostics)
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

TEST(CzddDickinsonTest, KeepsPackedSupportsBeyondOneWord)
{
    matrix_integer matrix;
    matrix.set_identity(65);
    matrix(63, 64) = integer(-2);
    matrix(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(matrix));
}

TEST(CzddDickinsonTest, CooperativeTimeoutEscapesDiagramOperations)
{
    request_timeout();
    EXPECT_THROW(model::czdd_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
