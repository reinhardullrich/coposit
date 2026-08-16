#include <coposit/model.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
size_t interval_recursive_uncovered_count(
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

TEST(IntervalRecursiveDickinsonTest, PreservesStrictDickinsonDecisions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(IntervalRecursiveDickinsonTest, ClassifiesCopositiveBoundary)
{
    const matrix_integer boundary = symmetric_matrix(4, {3, -1, -1, -1, 3, -1, -1, 3, -1, 3});
    const auto result = model::classify(boundary);
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
}

TEST(IntervalRecursiveDickinsonTest, OmitsACompleteBoundedInterval)
{
    // [001, 011] removes {0} at cardinality one and {0,1} at cardinality two, but not {0,2}.
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}};
    EXPECT_EQ(model::interval_recursive_uncovered_count(3, 1, intervals), 2U);
    EXPECT_EQ(model::interval_recursive_uncovered_count(3, 2, intervals), 2U);
}

TEST(IntervalRecursiveDickinsonTest, MatchesBruteForceForEveryPairOfFourDimensionalIntervals)
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
                EXPECT_EQ(model::interval_recursive_uncovered_count(dimension, cardinality, intervals),
                          brute_uncovered_count(dimension, cardinality, intervals));
        }
    }
}

TEST(IntervalRecursiveDickinsonTest, DoesNotEmitSupportsAlreadyCoveredByIdentityCertificates)
{
    matrix_integer identity;
    identity.set_identity(4);
    interval_recursive_dickinson_diagnostics::clear();
    EXPECT_TRUE(model::solve(identity));

    ASSERT_EQ(interval_recursive_dickinson_diagnostics::events.size(), 4U);
    for (const auto& event : interval_recursive_dickinson_diagnostics::events)
        EXPECT_EQ(event, (interval_recursive_dickinson_diagnostics::event{"process", 1}));
}

TEST(IntervalRecursiveDickinsonTest, KeepsPackedSupportsBeyondOneWord)
{
    matrix_integer matrix;
    matrix.set_identity(65);
    matrix(63, 64) = integer(-2);
    matrix(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(matrix));
}

} // namespace
