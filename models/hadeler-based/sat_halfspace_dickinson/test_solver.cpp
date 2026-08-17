#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>
#include <coposit/timeout.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
bool sat_halfspace_prefers_negative_singular_orientation_for_testing(size_t positive_products, size_t negative_products) noexcept;
size_t sat_halfspace_optimized_certificate_count_for_testing() noexcept;
size_t sat_halfspace_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t sat_halfspace_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
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

TEST(SatHalfspaceDickinsonTest, PreservesExactCombinedClassifications)
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

TEST(SatHalfspaceDickinsonTest, SelectsAnExactImprovingHalfspaceDirection)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14})));
    EXPECT_GT(model::sat_halfspace_optimized_certificate_count_for_testing(), 0U);
}

TEST(SatHalfspaceDickinsonTest, ChoosesTheSingularOrientationWithTheLargerUpperSet)
{
    EXPECT_TRUE(model::sat_halfspace_prefers_negative_singular_orientation_for_testing(1, 2));
    EXPECT_FALSE(model::sat_halfspace_prefers_negative_singular_orientation_for_testing(2, 1));
    EXPECT_FALSE(model::sat_halfspace_prefers_negative_singular_orientation_for_testing(2, 2));
}

TEST(SatHalfspaceDickinsonTest, StoresTheUnionOfDickinsonIntervalsInSat)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::sat_halfspace_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::sat_halfspace_uncovered_count(3, 2, intervals), 1U);
}

TEST(SatHalfspaceDickinsonTest, AddsTheExistingCardinalityOutputOnlyToExpiringIntervals)
{
    EXPECT_EQ(model::sat_halfspace_interval_clause_size(8, 0b00000001, 0b00000111), 7U);
    EXPECT_EQ(model::sat_halfspace_interval_clause_size(8, 0b00000001, 0b11111111), 1U);
}

TEST(SatHalfspaceDickinsonTest, PublishesTheChosenCertificateDistribution)
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

TEST(SatHalfspaceDickinsonTest, RecordsFactoredSingularSupportsOnlyWhenDiagnosticsAreEnabled)
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

TEST(SatHalfspaceDickinsonTest, CooperativeTimeoutInterruptsSat)
{
    request_timeout();
    EXPECT_THROW(model::sat_halfspace_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
