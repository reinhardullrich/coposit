#include <coposit/model.hpp>
#include <coposit/nbc_upward_supports.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <vector>

using namespace coposit;

namespace coposit::model {
size_t nbc_b6_pair_upward_count_for_testing() noexcept;
size_t nbc_b6_optimized_certificate_count_for_testing() noexcept;
bool nbc_b6_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool nbc_b6_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper);
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

struct count_state {
    size_t count = 0;
};

bool count_support(void* opaque, const std::vector<size_t>&)
{
    ++static_cast<count_state*>(opaque)->count;
    return true;
}

bool collect_support(void* opaque, const std::vector<size_t>& indices)
{
    size_t mask = 0;
    for (const size_t index : indices) mask |= size_t{1} << index;
    static_cast<std::vector<size_t>*>(opaque)->push_back(mask);
    return true;
}

std::vector<size_t> collect_layer(nbc_upward_supports& supports, size_t cardinality)
{
    std::vector<size_t> masks;
    EXPECT_EQ(supports.enumerate_cardinality(cardinality, &masks, &collect_support),
              nbc_upward_supports::enumeration_result::exhausted);
    std::sort(masks.begin(), masks.end());
    return masks;
}

bool stop_after_first_support(void* opaque, const std::vector<size_t>&)
{
    ++static_cast<count_state*>(opaque)->count;
    return false;
}

size_t count_layer(nbc_upward_supports& supports, size_t cardinality)
{
    count_state state;
    EXPECT_EQ(supports.enumerate_cardinality(cardinality, &state, &count_support),
              nbc_upward_supports::enumeration_result::exhausted);
    return state.count;
}

struct deferred_state {
    nbc_upward_supports& supports;
    size_t count = 0;
};

bool retain_smaller_root(void* opaque, const std::vector<size_t>& indices)
{
    auto& state = *static_cast<deferred_state*>(opaque);
    ++state.count;
    state.supports.add_upward_closure({indices.front()});
    return true;
}

TEST(NbcB6Test, EnumeratesEachSupportInOneCardinalityExactlyOnce)
{
    nbc_upward_supports supports(5);
    std::vector<size_t> expected;
    for (size_t first = 0; first < 5; ++first)
        for (size_t second = first + 1; second < 5; ++second)
            expected.push_back((size_t{1} << first) | (size_t{1} << second));
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(collect_layer(supports, 2), expected);
}

TEST(NbcB6Test, StreamsAlternatingCardinalitiesWithoutRepeatingSupports)
{
    nbc_upward_supports supports(5);
    supports.start_cardinality(2);
    supports.start_cardinality(4, true);

    std::vector<size_t> low_masks;
    std::vector<size_t> high_masks;
    std::vector<size_t> indices;
    bool low_open = true;
    bool high_open = true;
    while (low_open || high_open) {
        if (low_open) {
            low_open = supports.take_first(indices);
            if (low_open) collect_support(&low_masks, indices);
        }
        if (high_open) {
            high_open = supports.take_first(indices, true);
            if (high_open) collect_support(&high_masks, indices);
        }
    }

    std::sort(low_masks.begin(), low_masks.end());
    std::sort(high_masks.begin(), high_masks.end());
    EXPECT_EQ(low_masks.size(), 10U);
    EXPECT_EQ(high_masks.size(), 5U);
    EXPECT_EQ(std::adjacent_find(low_masks.begin(), low_masks.end()), low_masks.end());
    EXPECT_EQ(std::adjacent_find(high_masks.begin(), high_masks.end()), high_masks.end());
}

TEST(NbcB6Test, AppliesLiveCertificatesAndKeepsTheCursorAcrossCompaction)
{
    nbc_upward_supports supports(4);
    supports.start_cardinality(1);
    supports.start_cardinality(3, true);

    std::vector<size_t> first;
    ASSERT_TRUE(supports.take_first(first));
    ASSERT_EQ(first.size(), 1U);
    const size_t excluded = first.front();
    supports.add_upward_closure(first);

    std::vector<size_t> high;
    ASSERT_TRUE(supports.take_first(high, true));
    EXPECT_EQ(std::find(high.begin(), high.end(), excluded), high.end());

    supports.commit_frontiers(1, 4);
    std::vector<size_t> remaining;
    std::vector<size_t> next;
    while (supports.take_first(next)) {
        ASSERT_EQ(next.size(), 1U);
        remaining.push_back(next.front());
    }
    std::sort(remaining.begin(), remaining.end());
    EXPECT_EQ(remaining.size(), 3U);
    EXPECT_EQ(std::find(remaining.begin(), remaining.end(), excluded), remaining.end());
    EXPECT_EQ(std::adjacent_find(remaining.begin(), remaining.end()), remaining.end());
}

TEST(NbcB6Test, StopsEnumerationImmediatelyForAnExactWitness)
{
    nbc_upward_supports supports(5);
    count_state state;
    EXPECT_EQ(supports.enumerate_cardinality(2, &state, &stop_after_first_support),
              nbc_upward_supports::enumeration_result::stopped);
    EXPECT_EQ(state.count, 1U);
}

TEST(NbcB6Test, DefersEvenSmallerLowerEndpointsUntilTheLayerEnds)
{
    nbc_upward_supports supports(3);
    deferred_state state{supports};
    EXPECT_EQ(supports.enumerate_cardinality(2, &state, &retain_smaller_root),
              nbc_upward_supports::enumeration_result::exhausted);
    EXPECT_EQ(state.count, 3U);

    supports.commit_layer(2);
    EXPECT_EQ(count_layer(supports, 3), 0U);
}

TEST(NbcB6Test, AppliesRetainedPairClosuresToTheNextEnumeration)
{
    nbc_upward_supports supports(3);
    supports.add_pair_upward_closure(0, 1);
    supports.commit_layer(0);
    EXPECT_EQ(count_layer(supports, 2), 2U);
    EXPECT_EQ(count_layer(supports, 3), 0U);
}

TEST(NbcB6Test, CompactsCompleteSiblingClosuresAtTheLayerBoundary)
{
    nbc_upward_supports supports(3);
    supports.add_upward_closure({0, 1});
    supports.add_upward_closure({0, 2});
    supports.commit_layer(2);
    EXPECT_EQ(supports.interval_count(), 1U);
    EXPECT_EQ(count_layer(supports, 3), 0U);
}

TEST(NbcB6Test, CompactionPreservesTheExactOpenFutureSupports)
{
    constexpr size_t dimension = 4;
    std::vector<size_t> pairs;
    for (size_t first = 0; first < dimension; ++first)
        for (size_t second = first + 1; second < dimension; ++second)
            pairs.push_back((size_t{1} << first) | (size_t{1} << second));

    for (size_t family = 0; family < (size_t{1} << pairs.size()); ++family) {
        nbc_upward_supports supports(dimension);
        for (size_t pair = 0; pair < pairs.size(); ++pair) {
            if ((family & (size_t{1} << pair)) == 0) continue;
            std::vector<size_t> indices;
            for (size_t index = 0; index < dimension; ++index)
                if ((pairs[pair] & (size_t{1} << index)) != 0) indices.push_back(index);
            supports.add_upward_closure(indices);
        }
        supports.commit_layer(2);

        for (size_t cardinality = 3; cardinality <= dimension; ++cardinality) {
            std::vector<size_t> expected;
            for (size_t candidate = 1; candidate < (size_t{1} << dimension); ++candidate) {
                size_t count = 0;
                for (size_t bits = candidate; bits != 0; bits >>= 1) count += bits & 1;
                if (count != cardinality) continue;
                bool covered = false;
                for (size_t pair = 0; pair < pairs.size(); ++pair)
                    covered |= (family & (size_t{1} << pair)) != 0 && (candidate & pairs[pair]) == pairs[pair];
                if (!covered) expected.push_back(candidate);
            }
            EXPECT_EQ(collect_layer(supports, cardinality), expected);
        }
    }
}

TEST(NbcB6Test, DropsBoundedIntervalsAfterTheirLastPossibleLayer)
{
    nbc_upward_supports supports(3);
    support singleton(3);
    singleton.set(0);
    supports.add_interval(singleton, singleton);
    supports.commit_layer(0);
    EXPECT_EQ(supports.interval_count(), 1U);
    supports.commit_layer(1);
    EXPECT_EQ(supports.interval_count(), 0U);
}

TEST(NbcB6Test, CompactsNewIntervalsAgainstPreviouslyActiveIntervals)
{
    nbc_upward_supports supports(4);
    support inner_lower(4);
    support inner_upper(4);
    inner_lower.set(0);
    inner_upper.set(0);
    inner_upper.set(1);
    supports.add_interval(inner_lower, inner_upper);
    supports.commit_frontiers(1, 4);

    support outer_lower(4);
    support outer_upper(4);
    outer_upper.set(0);
    outer_upper.set(1);
    outer_upper.set(2);
    supports.add_interval(outer_lower, outer_upper);
    supports.commit_frontiers(1, 4);

    EXPECT_EQ(supports.interval_count(), 1U);
    EXPECT_EQ(count_layer(supports, 1), 1U);
    EXPECT_EQ(count_layer(supports, 2), 3U);
}

TEST(NbcB6Test, BuildsAnExactHalfspaceRaysDickinsonInterval)
{
    matrix_integer identity;
    identity.set_identity(3);
    support lower(3);
    support upper(3);

    EXPECT_TRUE(model::nbc_b6_certificate_for_testing(identity, {0}, lower, upper));
    EXPECT_TRUE(lower.contains(0));
    EXPECT_FALSE(lower.contains(1));
    EXPECT_FALSE(lower.contains(2));
    EXPECT_TRUE(upper.contains(0));
    EXPECT_TRUE(upper.contains(1));
    EXPECT_TRUE(upper.contains(2));
}

TEST(NbcB6Test, RetainsTheExactHalfspaceRayOptimization)
{
    const matrix_integer matrix = symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14});
    bool improved = false;
    for (size_t mask = 1; mask < 16 && !improved; ++mask) {
        std::vector<size_t> indices;
        for (size_t index = 0; index < 4; ++index)
            if ((mask & (size_t{1} << index)) != 0) indices.push_back(index);
        (void)model::nbc_b6_check_support_for_testing(matrix, indices);
        improved = model::nbc_b6_optimized_certificate_count_for_testing() > 0;
    }
    EXPECT_TRUE(improved);
}

TEST(NbcB6Test, PreservesExactCombinedClassifications)
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

TEST(NbcB6Test, MatchesTheIndependentExactThreeByThreeCriterion)
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
                            EXPECT_EQ(actual.is_copositive, expected.is_copositive);
                            EXPECT_EQ(actual.is_strictly_copositive, expected.is_strictly_copositive);
                        }
                    }
                }
            }
        }
    }
}

TEST(NbcB6Test, AppliesThePairCurvaturePrepass)
{
    const auto result = model::classify(symmetric_matrix(2, {1, 2, 1}));
    EXPECT_TRUE(result.is_copositive);
    EXPECT_TRUE(result.is_strictly_copositive);
    EXPECT_EQ(model::nbc_b6_pair_upward_count_for_testing(), 1U);
}

} // namespace
