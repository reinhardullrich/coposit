#include <coposit/model.hpp>
#include <coposit/timeout.hpp>

#include "source_trace.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
std::pair<size_t, size_t> multithreaded_cbdd_zed_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t multithreaded_cbdd_zed_maximum_interval_chain(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
size_t multithreaded_cbdd_zed_batch_size(size_t dimension, size_t workers);
std::vector<uint64_t> multithreaded_cbdd_zed_maximal_block_masks(const matrix_integer& matrix, size_t target);
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

TEST(MultithreadedCbddZedDickinsonTest, PreservesStrictDickinsonDecisions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(MultithreadedCbddZedDickinsonTest, DistinguishesNonStrictFromStrictCopositivity)
{
    const auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
}

TEST(MultithreadedCbddZedDickinsonTest, ClassifiesBothPredicatesInOneTraversal)
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

TEST(MultithreadedCbddZedDickinsonTest, SubtractsTheUnionOfBoundedIntervals)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::multithreaded_cbdd_zed_uncovered_count(3, 1, intervals).first, 1U);
    EXPECT_EQ(model::multithreaded_cbdd_zed_uncovered_count(3, 2, intervals).first, 1U);
}

TEST(MultithreadedCbddZedDickinsonTest, CompressesAForcedZeroChain)
{
    EXPECT_EQ(model::multithreaded_cbdd_zed_maximum_interval_chain(8, 0, 0), 8U);
}

TEST(MultithreadedCbddZedDickinsonTest, RoundsFiveNDownToAWholeWorkerWave)
{
    EXPECT_EQ(model::multithreaded_cbdd_zed_batch_size(25, 4), 124U);
    EXPECT_EQ(model::multithreaded_cbdd_zed_batch_size(25, 8), 120U);
    EXPECT_EQ(model::multithreaded_cbdd_zed_batch_size(1, 8), 5U);
}

TEST(MultithreadedCbddZedDickinsonTest, RepresentsAWholeZedDownsetWithAnEmptyLowerEndpoint)
{
    const std::vector<std::pair<uint64_t, uint64_t>> downset{{0, 0b101}};
    EXPECT_EQ(model::multithreaded_cbdd_zed_uncovered_count(3, 1, downset).first, 1U);
    EXPECT_EQ(model::multithreaded_cbdd_zed_uncovered_count(3, 2, downset).first, 2U);
    EXPECT_EQ(model::multithreaded_cbdd_zed_uncovered_count(3, 3, downset).first, 1U);
}

TEST(MultithreadedCbddZedDickinsonTest, MatchesBruteForceForEveryPairOfFourDimensionalIntervals)
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
                EXPECT_EQ(model::multithreaded_cbdd_zed_uncovered_count(dimension, cardinality, intervals).first,
                          brute_uncovered_count(dimension, cardinality, intervals));
        }
    }
}

TEST(MultithreadedCbddZedDickinsonTest, PartitionMatchesEveryFiveVertexGraph)
{
    constexpr size_t dimension = 5;
    constexpr size_t edge_count = dimension * (dimension - 1) / 2;
    for (uint64_t graph = 0; graph < (uint64_t{1} << edge_count); ++graph) {
        matrix_integer matrix(dimension, dimension);
        for (size_t index = 0; index < dimension; ++index) matrix(index, index).set_one();
        size_t edge = 0;
        for (size_t row = 0; row < dimension; ++row) {
            for (size_t column = row + 1; column < dimension; ++column, ++edge) {
                matrix(row, column) = integer((graph & (uint64_t{1} << edge)) != 0 ? -1 : 1);
                matrix(column, row) = matrix(row, column);
            }
        }

        std::vector<uint64_t> expected;
        for (uint64_t candidate = 1; candidate < (uint64_t{1} << dimension); ++candidate) {
            bool clique = true;
            for (size_t row = 0; row < dimension; ++row) {
                for (size_t column = row + 1; column < dimension; ++column) {
                    if ((candidate & (uint64_t{1} << row)) != 0 && (candidate & (uint64_t{1} << column)) != 0)
                        clique &= matrix(row, column).sign() <= 0;
                }
            }
            if (!clique) continue;

            bool maximal = true;
            for (size_t vertex = 0; vertex < dimension; ++vertex) {
                if ((candidate & (uint64_t{1} << vertex)) != 0) continue;
                bool extends = true;
                for (size_t member = 0; member < dimension; ++member)
                    if ((candidate & (uint64_t{1} << member)) != 0) extends &= matrix(vertex, member).sign() <= 0;
                maximal &= !extends;
            }
            if (maximal && (candidate & (candidate - 1)) != 0) expected.push_back(candidate);
        }
        std::sort(expected.begin(), expected.end());

        for (const size_t target : {1U, 4U, 7U})
            EXPECT_EQ(model::multithreaded_cbdd_zed_maximal_block_masks(matrix, target), expected)
                << "graph=" << graph << " target=" << target;
    }
}

TEST(MultithreadedCbddZedDickinsonTest, AcceptsABoundaryMatrixInNonStrictMode)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), model::copositivity_mode::copositive));
}

TEST(MultithreadedCbddZedDickinsonTest, KeepsPackedSupportsBeyondOneWord)
{
    matrix_integer matrix;
    matrix.set_identity(65);
    matrix(63, 64) = integer(-2);
    matrix(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(matrix));
}

TEST(MultithreadedCbddZedDickinsonTest, CooperativeTimeoutEscapesDiagramOperations)
{
    request_timeout();
    EXPECT_THROW(model::multithreaded_cbdd_zed_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
