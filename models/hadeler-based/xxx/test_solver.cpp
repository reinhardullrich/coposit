#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
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
bool sat_halfspace_rays_prefers_negative_singular_orientation_for_testing(size_t positive_products,
                                                                          size_t negative_products) noexcept;
size_t sat_halfspace_rays_optimized_certificate_count_for_testing() noexcept;
size_t sat_halfspace_rays_combined_ray_sweep_count_for_testing() noexcept;
size_t sat_halfspace_rays_combined_ray_improvement_count_for_testing() noexcept;
size_t sat_halfspace_rays_shortlist_limit_for_testing(size_t matrix_dimension, size_t support_dimension);
bool sat_halfspace_rays_prefers_ray_candidate_for_testing(size_t candidate_upper, size_t candidate_width, size_t candidate_gains,
                                                          size_t candidate_losses, size_t current_upper, size_t current_width,
                                                          size_t current_gains, size_t current_losses);
bool sat_halfspace_rays_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool sat_halfspace_rays_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper);
size_t sat_halfspace_rays_fixed_support_upper_size_for_testing() noexcept;
size_t sat_halfspace_rays_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t sat_halfspace_rays_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
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

TEST(SatHalfspaceRaysDickinsonTest, PreservesExactCombinedClassifications)
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

TEST(SatHalfspaceRaysDickinsonTest, SelectsAnExactImprovingHalfspaceDirection)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14})));
    EXPECT_GT(model::sat_halfspace_rays_optimized_certificate_count_for_testing(), 0U);
}

TEST(SatHalfspaceRaysDickinsonTest, ChoosesTheSingularOrientationWithTheLargerUpperSet)
{
    EXPECT_TRUE(model::sat_halfspace_rays_prefers_negative_singular_orientation_for_testing(1, 2));
    EXPECT_FALSE(model::sat_halfspace_rays_prefers_negative_singular_orientation_for_testing(2, 1));
    EXPECT_FALSE(model::sat_halfspace_rays_prefers_negative_singular_orientation_for_testing(2, 2));
}

TEST(SatHalfspaceRaysDickinsonTest, UsesWidthAsTheSecondaryObjective)
{
    const matrix_integer matrix = symmetric_matrix(2, {2, 1, 2});
    EXPECT_TRUE(model::sat_halfspace_rays_check_support_for_testing(matrix, {0, 1}));
    EXPECT_EQ(model::sat_halfspace_rays_fixed_support_upper_size_for_testing(), 2U);
    EXPECT_GT(model::sat_halfspace_rays_optimized_certificate_count_for_testing(), 0U);
}

TEST(SatHalfspaceRaysDickinsonTest, ExposesTheExactOptimizedFixedSupportCertificate)
{
    matrix_integer identity;
    identity.set_identity(3);
    support lower(3);
    support upper(3);

    EXPECT_TRUE(model::sat_halfspace_rays_certificate_for_testing(identity, {0}, lower, upper));
    EXPECT_TRUE(lower.contains(0));
    EXPECT_FALSE(lower.contains(1));
    EXPECT_FALSE(lower.contains(2));
    EXPECT_TRUE(upper.contains(0));
    EXPECT_TRUE(upper.contains(1));
    EXPECT_TRUE(upper.contains(2));
}

TEST(SatHalfspaceRaysDickinsonTest, AdaptiveShortlistRemainsBounded)
{
    EXPECT_EQ(model::sat_halfspace_rays_shortlist_limit_for_testing(5, 5), 5U);
    EXPECT_EQ(model::sat_halfspace_rays_shortlist_limit_for_testing(80, 80), 27U);
    EXPECT_EQ(model::sat_halfspace_rays_shortlist_limit_for_testing(3000, 3000), 64U);
    EXPECT_EQ(model::sat_halfspace_rays_shortlist_limit_for_testing(3000, 10), 10U);
}

TEST(SatHalfspaceRaysDickinsonTest, RayRetentionNeverTradesWidthForIncidentalGains)
{
    EXPECT_FALSE(model::sat_halfspace_rays_prefers_ray_candidate_for_testing(20, 4, 8, 0, 20, 5, 1, 0));
    EXPECT_TRUE(model::sat_halfspace_rays_prefers_ray_candidate_for_testing(20, 5, 8, 0, 20, 5, 1, 0));
}

TEST(SatHalfspaceRaysDickinsonTest, CombinedRayEscapesACoordinateWiseUpperMaximum)
{
    matrix_integer matrix(6, 6);
    for (size_t index = 0; index < 3; ++index) matrix(index, index) = integer(1);
    for (size_t index = 3; index < 6; ++index) matrix(index, index) = integer(10);
    const slong outside[3][3]{{-2, 2, 1}, {2, -2, 1}, {1, 1, -3}};
    for (size_t row = 0; row < 3; ++row) {
        for (size_t column = 0; column < 3; ++column) {
            matrix(row + 3, column) = integer(outside[row][column]);
            matrix(column, row + 3) = matrix(row + 3, column);
        }
    }

    EXPECT_TRUE(model::sat_halfspace_rays_check_support_for_testing(matrix, {0, 1, 2}));
    EXPECT_EQ(model::sat_halfspace_rays_fixed_support_upper_size_for_testing(), 6U);
    EXPECT_EQ(model::sat_halfspace_rays_combined_ray_sweep_count_for_testing(), 1U);
    EXPECT_EQ(model::sat_halfspace_rays_combined_ray_improvement_count_for_testing(), 1U);
}

TEST(SatHalfspaceRaysDickinsonTest, SearchesAtMostTheBestTwoDistinctCombinedRays)
{
    matrix_integer matrix(10, 10);
    for (size_t index = 0; index < 4; ++index) matrix(index, index) = integer(1);
    for (size_t index = 4; index < 10; ++index) matrix(index, index) = integer(10);
    const slong outside[6][4]{
        {-2, 2, 1, 0}, {2, -2, 1, 0}, {1, 1, -3, 0}, {1, 0, -2, 2}, {1, 0, 2, -2}, {-3, 0, 1, 1}};
    for (size_t row = 0; row < 6; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            matrix(row + 4, column) = integer(outside[row][column]);
            matrix(column, row + 4) = matrix(row + 4, column);
        }
    }

    EXPECT_TRUE(model::sat_halfspace_rays_check_support_for_testing(matrix, {0, 1, 2, 3}));
    EXPECT_EQ(model::sat_halfspace_rays_combined_ray_sweep_count_for_testing(), 2U);
}

TEST(SatHalfspaceRaysDickinsonTest, StoresTheUnionOfDickinsonIntervalsInSat)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::sat_halfspace_rays_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::sat_halfspace_rays_uncovered_count(3, 2, intervals), 1U);
}

TEST(SatHalfspaceRaysDickinsonTest, AddsTheExistingCardinalityOutputOnlyToExpiringIntervals)
{
    EXPECT_EQ(model::sat_halfspace_rays_interval_clause_size(8, 0b00000001, 0b00000111), 7U);
    EXPECT_EQ(model::sat_halfspace_rays_interval_clause_size(8, 0b00000001, 0b11111111), 1U);
}

TEST(SatHalfspaceRaysDickinsonTest, PublishesTheChosenCertificateDistribution)
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

TEST(SatHalfspaceRaysDickinsonTest, RecordsFactoredSingularSupportsOnlyWhenDiagnosticsAreEnabled)
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

TEST(SatHalfspaceRaysDickinsonTest, CooperativeTimeoutInterruptsSat)
{
    request_timeout();
    EXPECT_THROW(model::sat_halfspace_rays_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
