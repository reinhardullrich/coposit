#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <map>
#include <string>
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
std::array<size_t, 4> xxx_buffered_path_for_testing(const matrix_integer& matrix, const std::vector<size_t>& seed);
bool sat_halfspace_rays_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper);
size_t sat_halfspace_rays_fixed_support_upper_size_for_testing() noexcept;
size_t sat_halfspace_rays_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t sat_halfspace_rays_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
size_t xxx_cardinality_at_for_testing(size_t dimension, size_t layer);
bool xxx_support_available_for_testing(
    size_t dimension, uint64_t lower_mask, uint64_t upper_mask, uint64_t candidate_mask);
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

matrix_integer interval_regression_matrix()
{
    return symmetric_matrix(6, {
        4427705165, -4340027835, 3990667397, 4164566835, -4427705165, 4340027835,
        4427705165, -4291468083, 3782827437, 4340027835, -4427705165,
        4427705165, -4232842835, 3540117165, 4291468083,
        4427705165, -4164566835, 3265889540,
        4427705165, -4087112460,
        4427705165,
    });
}

TEST(XxxTest, PreservesExactCombinedClassifications)
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

TEST(XxxTest, AgreesWithTheCompleteOrderTwoCriterion)
{
    for (slong first_diagonal = -2; first_diagonal <= 2; ++first_diagonal) {
        for (slong off_diagonal = -2; off_diagonal <= 2; ++off_diagonal) {
            for (slong second_diagonal = -2; second_diagonal <= 2; ++second_diagonal) {
                SCOPED_TRACE(::testing::Message() << "a=" << first_diagonal << " b=" << off_diagonal
                                                  << " c=" << second_diagonal);
                model::copositivity_classification result;
                try {
                    result = model::classify(symmetric_matrix(2, {first_diagonal, off_diagonal, second_diagonal}));
                } catch (const std::exception& error) {
                    ADD_FAILURE() << error.what();
                    return;
                }
                const bool nonnegative_diagonal = first_diagonal >= 0 && second_diagonal >= 0;
                const bool positive_diagonal = first_diagonal > 0 && second_diagonal > 0;
                const slong squared_off_diagonal = off_diagonal * off_diagonal;
                const slong diagonal_product = first_diagonal * second_diagonal;
                const bool expected_copositive =
                    nonnegative_diagonal && (off_diagonal >= 0 || squared_off_diagonal <= diagonal_product);
                const bool expected_strict =
                    positive_diagonal && (off_diagonal >= 0 || squared_off_diagonal < diagonal_product);
                EXPECT_EQ(result.is_copositive, expected_copositive);
                EXPECT_EQ(result.is_strictly_copositive, expected_strict);
            }
        }
    }
}

TEST(XxxTest, SelectsAnExactImprovingHalfspaceDirection)
{
    const matrix_integer matrix = symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14});
    bool improved = false;
    for (uint64_t mask = 1; mask < (uint64_t{1} << matrix.rows()) && !improved; ++mask) {
        std::vector<size_t> indices;
        for (size_t index = 0; index < matrix.rows(); ++index)
            if ((mask & (uint64_t{1} << index)) != 0) indices.push_back(index);
        model::sat_halfspace_rays_check_support_for_testing(matrix, indices);
        improved = model::sat_halfspace_rays_optimized_certificate_count_for_testing() > 0;
    }
    EXPECT_TRUE(improved);
}

TEST(XxxTest, ChoosesTheSingularOrientationWithTheLargerUpperSet)
{
    EXPECT_TRUE(model::sat_halfspace_rays_prefers_negative_singular_orientation_for_testing(1, 2));
    EXPECT_FALSE(model::sat_halfspace_rays_prefers_negative_singular_orientation_for_testing(2, 1));
    EXPECT_FALSE(model::sat_halfspace_rays_prefers_negative_singular_orientation_for_testing(2, 2));
}

TEST(XxxTest, UsesWidthAsTheSecondaryObjective)
{
    const matrix_integer matrix = symmetric_matrix(2, {2, 1, 2});
    EXPECT_TRUE(model::sat_halfspace_rays_check_support_for_testing(matrix, {0, 1}));
    EXPECT_EQ(model::sat_halfspace_rays_fixed_support_upper_size_for_testing(), 2U);
    EXPECT_GT(model::sat_halfspace_rays_optimized_certificate_count_for_testing(), 0U);
}

TEST(XxxTest, ExposesTheExactOptimizedFixedSupportCertificate)
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

TEST(XxxTest, AdaptiveShortlistRemainsBounded)
{
    EXPECT_EQ(model::sat_halfspace_rays_shortlist_limit_for_testing(5, 5), 5U);
    EXPECT_EQ(model::sat_halfspace_rays_shortlist_limit_for_testing(80, 80), 27U);
    EXPECT_EQ(model::sat_halfspace_rays_shortlist_limit_for_testing(3000, 3000), 64U);
    EXPECT_EQ(model::sat_halfspace_rays_shortlist_limit_for_testing(3000, 10), 10U);
}

TEST(XxxTest, RayRetentionNeverTradesWidthForIncidentalGains)
{
    EXPECT_FALSE(model::sat_halfspace_rays_prefers_ray_candidate_for_testing(20, 4, 8, 0, 20, 5, 1, 0));
    EXPECT_TRUE(model::sat_halfspace_rays_prefers_ray_candidate_for_testing(20, 5, 8, 0, 20, 5, 1, 0));
}

TEST(XxxTest, CombinedRayEscapesACoordinateWiseUpperMaximum)
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

TEST(XxxTest, SearchesAtMostTheBestTwoDistinctCombinedRays)
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

TEST(XxxTest, StoresTheUnionOfDickinsonIntervalsInSat)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::sat_halfspace_rays_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::sat_halfspace_rays_uncovered_count(3, 2, intervals), 1U);
}

TEST(XxxTest, AddsTheExistingCardinalityOutputOnlyToExpiringIntervals)
{
    EXPECT_EQ(model::sat_halfspace_rays_interval_clause_size(8, 0b00000001, 0b00000111), 7U);
    EXPECT_EQ(model::sat_halfspace_rays_interval_clause_size(8, 0b00000001, 0b11111111), 1U);
}

TEST(XxxTest, AlternatesAndExhaustsCardinalityLayersFromBothEnds)
{
    const std::vector<size_t> expected{1, 7, 2, 6, 3, 5, 4};
    for (size_t layer = 0; layer < expected.size(); ++layer)
        EXPECT_EQ(model::xxx_cardinality_at_for_testing(7, layer), expected[layer]);
}

TEST(XxxTest, SatRejectsSupportsCoveredByCommittedIntervals)
{
    constexpr size_t dimension = 4;
    constexpr uint64_t full = (uint64_t{1} << dimension) - 1;
    for (uint64_t lower = 0; lower <= full; ++lower) {
        for (uint64_t upper = 0; upper <= full; ++upper) {
            if ((lower & ~upper) != 0) continue;
            for (uint64_t candidate = 1; candidate <= full; ++candidate) {
                const bool covered = (candidate & lower) == lower && (candidate & ~upper) == 0;
                EXPECT_EQ(model::xxx_support_available_for_testing(dimension, lower, upper, candidate), !covered)
                    << "lower=" << lower << " upper=" << upper << " candidate=" << candidate;
            }
        }
    }
}

TEST(XxxTest, DelaysPathIntervalsUntilKktThenCommitsTheWholePath)
{
    const matrix_integer matrix = symmetric_matrix(2, {2, 1, 1});
    const auto [visited, committed, reached_kkt, seed_available] = model::xxx_buffered_path_for_testing(matrix, {0});

    EXPECT_EQ(visited, 2U);
    EXPECT_EQ(committed, 5U);
    EXPECT_EQ(reached_kkt, 1U);
    EXPECT_EQ(seed_available, 0U);
}

TEST(XxxTest, CommitsTheWholePathWhenNoOpenPreferredSuccessorRemains)
{
    const matrix_integer matrix = symmetric_matrix(2, {-2, 0, 1});
    const auto [visited, committed, reached_kkt, seed_available] = model::xxx_buffered_path_for_testing(matrix, {1});

    EXPECT_EQ(visited, 2U);
    EXPECT_EQ(committed, 3U);
    EXPECT_EQ(reached_kkt, 0U);
    EXPECT_EQ(seed_available, 0U);
}

TEST(XxxTest, EveryVisitedSupportContributesItsOptimizedDickinsonInterval)
{
    const matrix_integer matrix = interval_regression_matrix();
    support lower(matrix.rows());
    support upper(matrix.rows());
    ASSERT_TRUE(model::sat_halfspace_rays_certificate_for_testing(matrix, {0, 1}, lower, upper));

    for (size_t index = 0; index < matrix.rows(); ++index) {
        EXPECT_EQ(lower.contains(index), index == 0 || index == 1);
        EXPECT_EQ(upper.contains(index), index == 0 || index == 1 || index == 3);
    }
}

TEST(XxxTest, OrdinaryIntervalsCompleteTheFormerStalledSmokeCase)
{
    const model::copositivity_classification result = model::classify(interval_regression_matrix());
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
}

TEST(XxxTest, PublishesTheChosenCertificateDistribution)
{
    matrix_integer identity;
    identity.set_identity(2);

    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_TRUE(model::solve(identity));
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_FALSE(snapshot.certificate_cardinality_free_index_upper_size_counts.empty());
}

TEST(XxxTest, RecordsFactoredSingularSupportsOnlyWhenDiagnosticsAreEnabled)
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

TEST(XxxTest, RecordsExactKktSupportWhenDiagnosticsAreEnabled)
{
    matrix_integer identity;
    identity.set_identity(1);

    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_TRUE(model::solve(identity));
    const std::string events = diagnostics::detail::load_diagnostics();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_NE(events.find("event=xxx_face_stationary k=1 support=[1] positive_support=[1] kkt=yes psd=yes nullity=0 payoff_sign=1"),
              std::string::npos);
}

TEST(XxxTest, RecordsTheTerminalNegativeSupportWhenDiagnosticsAreEnabled)
{
    const matrix_integer negative = symmetric_matrix(1, {-1});

    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_FALSE(model::solve(negative));
    const std::string events = diagnostics::detail::load_diagnostics();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_NE(events.find("event=xxx_terminal outcome=dickinson_negative k=1 support=[1]"), std::string::npos);
}

TEST(XxxTest, CooperativeTimeoutInterruptsSat)
{
    request_timeout();
    EXPECT_THROW(model::sat_halfspace_rays_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
