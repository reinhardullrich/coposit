#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include <coposit/support.hpp>

using coposit::support;
using coposit::support_context;

namespace coposit {

struct support_context_test_access {
    static size_t allocation_count(const support_context& context) noexcept { return context.allocations_.size(); }
};

} // namespace coposit

TEST(SupportContextTest, SmallAndLargeRepresentationsHaveTheSameSemantics)
{
    for (const size_t dimension : {1u, 20u, 63u, 64u, 65u, 127u, 128u, 129u, 260u}) {
        support_context context(dimension);
        support selected = context.make();
        std::vector<size_t> expected_indices{0, dimension / 2, dimension - 1};
        std::sort(expected_indices.begin(), expected_indices.end());
        expected_indices.erase(std::unique(expected_indices.begin(), expected_indices.end()), expected_indices.end());
        for (const size_t position : expected_indices) context.set(selected, position);

        EXPECT_EQ(context.count(selected), expected_indices.size());
        EXPECT_EQ(context.first(selected), expected_indices.front());
        std::vector<size_t> indices;
        context.extract_set_indices(selected, indices);
        EXPECT_EQ(indices, expected_indices);

        std::vector<size_t> expected_unset;
        for (size_t position = 0; position < dimension; ++position)
            if (!std::binary_search(expected_indices.begin(), expected_indices.end(), position)) expected_unset.push_back(position);
        context.extract_unset_indices(selected, indices);
        EXPECT_EQ(indices, expected_unset);

        support clone = context.clone(selected);
        context.reset(clone, dimension / 2);
        EXPECT_TRUE(context.is_subset_of(clone, selected));
        if (dimension > 1) EXPECT_FALSE(context.equal(clone, selected));

        support rotated = context.clone(selected);
        context.rotate_one_right(rotated);
        std::vector<size_t> expected_rotated;
        for (const size_t position : expected_indices) expected_rotated.push_back((position + dimension - 1) % dimension);
        std::sort(expected_rotated.begin(), expected_rotated.end());
        context.extract_set_indices(rotated, indices);
        EXPECT_EQ(indices, expected_rotated);
        for (size_t shift = 1; shift < dimension; ++shift) context.rotate_one_right(rotated);
        EXPECT_TRUE(context.equal(rotated, selected));

        support reflected = context.clone(selected);
        context.reflect(reflected);
        std::vector<size_t> expected_reflected;
        for (const size_t position : expected_indices) expected_reflected.push_back(dimension - 1 - position);
        std::sort(expected_reflected.begin(), expected_reflected.end());
        context.extract_set_indices(reflected, indices);
        EXPECT_EQ(indices, expected_reflected);
        context.reflect(reflected);
        EXPECT_TRUE(context.equal(reflected, selected));

        support copied = context.make();
        context.copy(copied, selected);
        EXPECT_TRUE(context.equal(copied, selected));

        support combined = context.clone(clone);
        context.add(combined, selected);
        EXPECT_TRUE(context.equal(combined, selected));

        support low = context.make();
        support high = context.make();
        context.set(low, 0);
        context.set(high, dimension - 1);
        if (dimension > 1) EXPECT_TRUE(context.less(low, high));
        context.swap(low, high);
        EXPECT_TRUE(context.contains(low, dimension - 1));
        EXPECT_TRUE(context.contains(high, 0));
        context.clear(high);
        EXPECT_TRUE(context.empty(high));

        support complement = context.make();
        context.set_all(complement);
        context.subtract(complement, selected);
        EXPECT_EQ(context.count(complement), dimension - expected_indices.size());

        support intersection = context.clone(selected);
        context.intersect(intersection, clone);
        EXPECT_TRUE(context.equal(intersection, clone));
    }
}

TEST(SupportContextTest, ReusesReleasedLargeStorage)
{
    support_context context(65);
    support first = context.make();
    context.set(first, 64);
    EXPECT_EQ(coposit::support_context_test_access::allocation_count(context), 1U);

    context.release(std::move(first));
    context.release(std::move(first));

    support reused = context.make();
    EXPECT_EQ(coposit::support_context_test_access::allocation_count(context), 1U);
    EXPECT_TRUE(context.empty(reused));

    context.set(reused, 64);
    context.release(std::move(reused));
    support reused_again = context.make();
    EXPECT_EQ(coposit::support_context_test_access::allocation_count(context), 1U);
    EXPECT_TRUE(context.empty(reused_again));
}

TEST(SupportContextTest, RejectsZeroDimension)
{
    EXPECT_THROW(support_context(0), std::invalid_argument);
}
