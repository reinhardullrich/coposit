#include <coposit/model.hpp>

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
size_t dense_bitset_required_bytes(size_t dimension);
uint64_t dense_bitset_future_clear_count(
    size_t dimension, size_t cardinality, size_t visited, uint64_t lower, uint64_t upper);
std::vector<uint64_t> dense_bitset_remaining_masks(
    size_t dimension, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
}

namespace {

void use_test_limit()
{
    ASSERT_EQ(setenv("COPOSIT_DENSE_BITSET_MAX_N", "16", 1), 0);
    ASSERT_EQ(unsetenv("COPOSIT_DENSE_BITSET_MAX_GIB"), 0);
}

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

matrix_integer constant_off_diagonal(size_t dimension, slong diagonal, slong off_diagonal)
{
    matrix_integer matrix(dimension, dimension);
    for (size_t row = 0; row < dimension; ++row)
        for (size_t column = 0; column < dimension; ++column)
            matrix(row, column) = integer(row == column ? diagonal : off_diagonal);
    return matrix;
}

size_t popcount(uint64_t value)
{
    size_t result = 0;
    while (value != 0) {
        value &= value - 1;
        ++result;
    }
    return result;
}

std::vector<uint64_t> brute_remaining(
    size_t dimension, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    std::vector<uint64_t> result;
    for (size_t cardinality = 1; cardinality <= dimension; ++cardinality) {
        for (uint64_t candidate = 1; candidate < (uint64_t{1} << dimension); ++candidate) {
            if (popcount(candidate) != cardinality) continue;
            bool covered = false;
            for (const auto& interval : intervals)
                covered |= (interval.first & ~candidate) == 0 && (candidate & ~interval.second) == 0;
            if (!covered) result.push_back(candidate);
        }
    }
    return result;
}

TEST(DenseBitsetDickinsonTest, PreservesBothDickinsonPredicates)
{
    use_test_limit();
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));

    constexpr auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));

    const model::copositivity_classification boundary = model::classify(constant_off_diagonal(4, 3, -1));
    EXPECT_TRUE(boundary.is_copositive);
    EXPECT_FALSE(boundary.is_strictly_copositive);
}

TEST(DenseBitsetDickinsonTest, UsesTheRequestedPackedMemory)
{
    EXPECT_EQ(model::dense_bitset_required_bytes(30), size_t{1} << 27);
    EXPECT_EQ(model::dense_bitset_required_bytes(33), size_t{1} << 30);
    EXPECT_EQ(model::dense_bitset_required_bytes(36), size_t{1} << 33);
}

TEST(DenseBitsetDickinsonTest, ClearsExactIntervalsAndTraversesByCardinality)
{
    use_test_limit();
    constexpr size_t dimension = 4;
    for (uint64_t lower = 1; lower < (uint64_t{1} << dimension); ++lower) {
        for (uint64_t upper = lower; upper < (uint64_t{1} << dimension); ++upper) {
            if ((lower & ~upper) != 0) continue;
            const std::vector<std::pair<uint64_t, uint64_t>> intervals{{lower, upper}};
            EXPECT_EQ(model::dense_bitset_remaining_masks(dimension, intervals), brute_remaining(dimension, intervals));
        }
    }
}

TEST(DenseBitsetDickinsonTest, ClearsOverlappingIntervalsOnlyOnce)
{
    use_test_limit();
    constexpr size_t dimension = 5;
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b00001, 0b11111}, {0b00110, 0b11110}, {0b01001, 0b11011}};
    EXPECT_EQ(model::dense_bitset_remaining_masks(dimension, intervals), brute_remaining(dimension, intervals));
}

TEST(DenseBitsetDickinsonTest, CoversOnlySupportsAfterTheTraversalCursor)
{
    use_test_limit();
    constexpr size_t dimension = 5;
    constexpr size_t cardinality = 3;
    constexpr size_t visited = 4;
    constexpr uint64_t lower = 0b00010;
    constexpr uint64_t upper = 0b11111;

    uint64_t expected = 0;
    size_t ordinal = 0;
    for (uint64_t candidate = 1; candidate < (uint64_t{1} << dimension); ++candidate) {
        const size_t candidate_cardinality = popcount(candidate);
        if (candidate_cardinality < cardinality) continue;
        if (candidate_cardinality == cardinality && ++ordinal <= visited) continue;
        expected += (lower & ~candidate) == 0 && (candidate & ~upper) == 0;
    }

    EXPECT_EQ(model::dense_bitset_future_clear_count(dimension, cardinality, visited, lower, upper), expected);
}

TEST(DenseBitsetDickinsonTest, JumpsOverIdentitySupersets)
{
    use_test_limit();
    matrix_integer identity;
    identity.set_identity(4);
    dense_bitset_dickinson_trace::clear();
    EXPECT_TRUE(model::solve(identity));

    ASSERT_EQ(dense_bitset_dickinson_trace::events.size(), 4U);
    for (const auto& event : dense_bitset_dickinson_trace::events)
        EXPECT_EQ(event, (dense_bitset_dickinson_trace::event{"process", 1}));
}

TEST(DenseBitsetDickinsonTest, EnforcesExactlyOneConfiguredLimit)
{
    matrix_integer identity;
    identity.set_identity(4);

    ASSERT_EQ(setenv("COPOSIT_DENSE_BITSET_MAX_N", "3", 1), 0);
    ASSERT_EQ(unsetenv("COPOSIT_DENSE_BITSET_MAX_GIB"), 0);
    EXPECT_THROW(model::solve(identity), std::length_error);

    ASSERT_EQ(setenv("COPOSIT_DENSE_BITSET_MAX_N", "4", 1), 0);
    ASSERT_EQ(setenv("COPOSIT_DENSE_BITSET_MAX_GIB", "1", 1), 0);
    EXPECT_THROW(model::solve(identity), std::invalid_argument);
    use_test_limit();
}

} // namespace
