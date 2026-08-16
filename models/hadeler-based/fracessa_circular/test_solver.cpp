#include "circular_affine_symmetry.hpp"
#include "circular_support_generator.hpp"

#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

using namespace coposit;
using namespace coposit::model::fracessa_circular_detail;

namespace {

matrix_integer circular_matrix(std::initializer_list<slong> first_row)
{
    const std::vector<slong> values(first_row);
    matrix_integer matrix(values.size(), values.size());
    for (size_t row = 0; row < values.size(); ++row) {
        for (size_t column = 0; column < values.size(); ++column) {
            matrix(row, column) = integer(values[(column + values.size() - row) % values.size()]);
        }
    }
    return matrix;
}

std::uint64_t mask(const support& value)
{
    std::vector<size_t> indices;
    value.copy_indices_to(indices);
    std::uint64_t result = 0;
    for (const size_t index : indices) result |= std::uint64_t{1} << index;
    return result;
}

} // namespace

TEST(FracessaCircularModelTest, DistinguishesAllThreeMinimumSignsWithNonzeroDiagonal)
{
    const model::copositivity_classification positive = model::classify(circular_matrix({2, 0, 0, 0}));
    EXPECT_TRUE(positive.is_copositive);
    EXPECT_TRUE(positive.is_strictly_copositive);

    const model::copositivity_classification zero = model::classify(circular_matrix({2, -1, 0, -1}));
    EXPECT_TRUE(zero.is_copositive);
    EXPECT_FALSE(zero.is_strictly_copositive);

    const model::copositivity_classification negative = model::classify(circular_matrix({1, -1, 0, -1}));
    EXPECT_FALSE(negative.is_copositive);
    EXPECT_FALSE(negative.is_strictly_copositive);

    EXPECT_TRUE(model::solve(circular_matrix({2, -1, 0, -1}), model::copositivity_mode::copositive));
    EXPECT_FALSE(model::solve(circular_matrix({2, -1, 0, -1}), model::copositivity_mode::strictly_copositive));
}

TEST(FracessaCircularModelTest, GeneratesEveryBraceletOnceAndExpandsCompleteOrbits)
{
    constexpr size_t dimension = 8;
    circular_support_generator generator(dimension);
    std::array<bool, std::uint64_t{1} << dimension> seen{};
    size_t representatives = 0;

    generator.generate([&](const support& representative, size_t) {
        ++representatives;
        std::array<bool, std::uint64_t{1} << dimension> orbit{};
        support current = representative;
        for (size_t shift = 0; shift < dimension; ++shift) {
            orbit[mask(current)] = true;
            current.rotate_one_right();
        }
        current = representative;
        current.reflect();
        for (size_t shift = 0; shift < dimension; ++shift) {
            orbit[mask(current)] = true;
            current.rotate_one_right();
        }
        for (size_t candidate = 1; candidate < orbit.size(); ++candidate) {
            if (!orbit[candidate]) continue;
            EXPECT_FALSE(seen[candidate]);
            seen[candidate] = true;
        }
        return true;
    });

    EXPECT_EQ(representatives, 29U);
    for (size_t candidate = 1; candidate < seen.size(); ++candidate) EXPECT_TRUE(seen[candidate]);
}

TEST(FracessaCircularModelTest, AppliesExactAffineMultiplierReduction)
{
    const matrix_integer matrix = circular_matrix({5, 7, 11, 7, 13, 7, 11, 7});
    circular_affine_symmetry symmetry(matrix);
    circular_support_generator generator(matrix.rows());
    size_t representatives = 0;
    generator.generate([&](const support& candidate, size_t) {
        if (symmetry.is_representative(candidate)) ++representatives;
        return true;
    });
    EXPECT_EQ(representatives, 23U);
}

TEST(FracessaCircularModelTest, SupportsCircularOrbitsAcrossMachineWords)
{
    matrix_integer matrix(70, 70);
    for (size_t row = 0; row < 70; ++row) {
        for (size_t column = 0; column < 70; ++column) matrix(row, column).set_one();
    }
    const model::copositivity_classification result = model::classify(matrix);
    EXPECT_TRUE(result.is_copositive);
    EXPECT_TRUE(result.is_strictly_copositive);
}

TEST(FracessaCircularModelTest, PublishesTruthfulBraceletDiagnostics)
{
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    const model::copositivity_classification result = model::classify(circular_matrix({5, 1, 0, 1}));
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_TRUE(result.is_copositive);
    EXPECT_TRUE(result.is_strictly_copositive);
    EXPECT_EQ(snapshot.kind, diagnostics::metric::bracelet);
    EXPECT_GT(snapshot.nodes, 0U);
    EXPECT_GT(snapshot.secondary, 0U);
    EXPECT_GT(snapshot.splits, 0U);
    EXPECT_LE(snapshot.current, snapshot.maximum);
}
