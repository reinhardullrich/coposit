#include <coposit/interval_supports.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

coposit::support make_support(coposit::support_context& context, std::initializer_list<size_t> indices)
{
    coposit::support result = context.make();
    for (const size_t index : indices) context.set(result, index);
    return result;
}

uint64_t mask(const std::vector<size_t>& indices)
{
    uint64_t result = 0;
    for (const size_t index : indices) result |= uint64_t{1} << index;
    return result;
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

std::pair<uint64_t, uint64_t> decode_interval(size_t dimension, size_t code)
{
    uint64_t lower = 0;
    uint64_t upper = 0;
    for (size_t index = 0; index < dimension; ++index) {
        const size_t state = code % 3;
        code /= 3;
        if (state != 0) upper |= uint64_t{1} << index;
        if (state == 2) lower |= uint64_t{1} << index;
    }
    return {lower, upper};
}

void add_mask_interval(coposit::support_context& context, coposit::interval_supports& supports, uint64_t lower_mask, uint64_t upper_mask)
{
    coposit::support lower = context.make();
    coposit::support upper = context.make();
    for (size_t index = 0; index < context.dimension(); ++index) {
        if ((lower_mask & (uint64_t{1} << index)) != 0) context.set(lower, index);
        if ((upper_mask & (uint64_t{1} << index)) != 0) context.set(upper, index);
    }
    supports.add_interval(lower, upper);
    context.release(std::move(lower));
    context.release(std::move(upper));
}

TEST(IntervalSupports, EnumeratesExactlyTheUncoveredCardinalitySlices)
{
    coposit::support_context context(6);
    coposit::interval_supports supports(context);

    coposit::support empty = context.make();
    coposit::support full = context.make();
    context.set_all(full);
    coposit::support upward = make_support(context, {0, 2});
    coposit::support downward = make_support(context, {0, 1, 3, 4});
    coposit::support middle_lower = make_support(context, {1});
    coposit::support middle_upper = make_support(context, {1, 2, 4});
    supports.add_interval(upward, full);
    supports.add_interval(empty, downward);
    supports.add_interval(middle_lower, middle_upper);

    for (size_t cardinality = 0; cardinality <= 6; ++cardinality) {
        std::set<uint64_t> expected;
        for (uint64_t candidate = 0; candidate < (uint64_t{1} << 6); ++candidate) {
            if (popcount(candidate) != cardinality) continue;
            const bool covered = ((candidate & 0b000101) == 0b000101) || ((candidate & ~uint64_t{0b011011}) == 0)
                || ((candidate & 0b000010) != 0 && (candidate & ~uint64_t{0b010110}) == 0);
            if (!covered) expected.insert(candidate);
        }

        supports.start_low_cardinality(cardinality);
        std::set<uint64_t> actual;
        std::vector<size_t> indices;
        while (supports.take_first_low(indices)) actual.insert(mask(indices));
        EXPECT_EQ(actual, expected) << "cardinality " << cardinality;
    }

    EXPECT_EQ(supports.interval_count(), 3);
    EXPECT_TRUE(supports.covers_interval(upward, full));

    context.release(std::move(empty));
    context.release(std::move(full));
    context.release(std::move(upward));
    context.release(std::move(downward));
    context.release(std::move(middle_lower));
    context.release(std::move(middle_upper));
}

TEST(IntervalSupports, MatchesBruteForceForEveryFiveDimensionalInterval)
{
    constexpr size_t dimension = 5;
    constexpr size_t interval_count = 3 * 3 * 3 * 3 * 3;
    for (size_t code = 0; code < interval_count; ++code) {
        const auto [lower, upper] = decode_interval(dimension, code);
        coposit::support_context context(dimension);
        coposit::interval_supports supports(context);
        add_mask_interval(context, supports, lower, upper);

        for (size_t cardinality = 0; cardinality <= dimension; ++cardinality) {
            std::set<uint64_t> actual;
            std::vector<size_t> indices;
            supports.start_low_cardinality(cardinality);
            while (supports.take_first_low(indices)) actual.insert(mask(indices));

            std::set<uint64_t> expected;
            for (uint64_t candidate = 0; candidate < (uint64_t{1} << dimension); ++candidate) {
                if (popcount(candidate) != cardinality) continue;
                if ((lower & ~candidate) != 0 || (candidate & ~upper) != 0) expected.insert(candidate);
            }
            ASSERT_EQ(actual, expected) << "interval code " << code << ", cardinality " << cardinality;
        }
    }
}

TEST(IntervalSupports, MatchesBruteForceForEveryPairOfThreeDimensionalIntervals)
{
    constexpr size_t dimension = 3;
    constexpr size_t interval_count = 3 * 3 * 3;
    for (size_t first_code = 0; first_code < interval_count; ++first_code) {
        const auto [first_lower, first_upper] = decode_interval(dimension, first_code);
        for (size_t second_code = 0; second_code < interval_count; ++second_code) {
            const auto [second_lower, second_upper] = decode_interval(dimension, second_code);
            coposit::support_context context(dimension);
            coposit::interval_supports supports(context);
            add_mask_interval(context, supports, first_lower, first_upper);
            add_mask_interval(context, supports, second_lower, second_upper);

            for (size_t cardinality = 0; cardinality <= dimension; ++cardinality) {
                std::set<uint64_t> actual;
                std::vector<size_t> indices;
                supports.start_low_cardinality(cardinality);
                while (supports.take_first_low(indices)) actual.insert(mask(indices));

                std::set<uint64_t> expected;
                for (uint64_t candidate = 0; candidate < (uint64_t{1} << dimension); ++candidate) {
                    if (popcount(candidate) != cardinality) continue;
                    const bool first_covers = (first_lower & ~candidate) == 0 && (candidate & ~first_upper) == 0;
                    const bool second_covers = (second_lower & ~candidate) == 0 && (candidate & ~second_upper) == 0;
                    if (!first_covers && !second_covers) expected.insert(candidate);
                }
                ASSERT_EQ(actual, expected) << "interval codes " << first_code << ", " << second_code << ", cardinality " << cardinality;
            }
        }
    }
}

TEST(IntervalSupports, AppliesNewIntervalsToBothLiveStreams)
{
    coposit::support_context context(5);
    coposit::interval_supports supports(context);
    supports.start_low_cardinality(2);
    supports.start_high_cardinality(3);

    std::vector<size_t> first;
    ASSERT_TRUE(supports.take_first_low(first));

    coposit::support lower = make_support(context, {0});
    coposit::support full = context.make();
    context.set_all(full);
    supports.add_interval(lower, full);

    std::vector<size_t> indices;
    while (supports.take_first_low(indices)) EXPECT_EQ(mask(indices) & 1, 0);
    while (supports.take_first_high(indices)) EXPECT_EQ(mask(indices) & 1, 0);

    context.release(std::move(lower));
    context.release(std::move(full));
}

TEST(IntervalSupports, SupportsDimensionsAboveOneMachineWord)
{
    coposit::support_context context(70);
    coposit::interval_supports supports(context);
    coposit::support lower = make_support(context, {1, 65});
    coposit::support full = context.make();
    context.set_all(full);
    supports.add_interval(lower, full);
    supports.start_low_cardinality(2);

    size_t count = 0;
    std::vector<size_t> indices;
    while (supports.take_first_low(indices)) {
        EXPECT_NE(indices, (std::vector<size_t>{1, 65}));
        ++count;
    }
    EXPECT_EQ(count, 70 * 69 / 2 - 1);

    context.release(std::move(lower));
    context.release(std::move(full));
}

TEST(IntervalSupports, RejectsInvalidInputAndHandlesTheUniversalInterval)
{
    coposit::support_context context(4);
    coposit::interval_supports supports(context);
    coposit::support lower = make_support(context, {0, 1});
    coposit::support upper = make_support(context, {0});
    EXPECT_THROW(supports.add_interval(lower, upper), std::invalid_argument);

    coposit::support empty = context.make();
    coposit::support full = context.make();
    context.set_all(full);
    supports.add_interval(lower, full);
    supports.add_interval(empty, full);
    EXPECT_EQ(supports.interval_count(), 1);

    context.release(std::move(lower));
    context.release(std::move(upper));
    context.release(std::move(empty));
    context.release(std::move(full));
}

} // namespace
