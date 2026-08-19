#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
std::vector<size_t> xxx_two_floating_successor_for_testing(const matrix_integer& matrix, const std::vector<size_t>& current,
                                                           const std::vector<std::vector<size_t>>& known);
void xxx_two_completed_path_seed_for_testing(const matrix_integer& matrix, const std::vector<size_t>& seed);
size_t xxx_two_exact_random_paths_for_testing(const matrix_integer& matrix, size_t requested, uint64_t random_seed);
std::array<size_t, 4> xxx_buffered_path_for_testing(const matrix_integer& matrix, const std::vector<size_t>& seed);
size_t sat_halfspace_rays_uncovered_count(
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

TEST(XxxTwoTest, PreservesExactCombinedClassifications)
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

TEST(XxxTwoTest, AgreesWithTheCompleteOrderTwoCriterionWhenTheWalkResolves)
{
    size_t resolved = 0;
    for (slong first_diagonal = 1; first_diagonal <= 2; ++first_diagonal) {
        for (slong off_diagonal = -2; off_diagonal <= 2; ++off_diagonal) {
            for (slong second_diagonal = 1; second_diagonal <= 2; ++second_diagonal) {
                SCOPED_TRACE(::testing::Message() << "a=" << first_diagonal << " b=" << off_diagonal
                                                  << " c=" << second_diagonal);
                model::copositivity_classification result;
                try {
                    result = model::classify(symmetric_matrix(2, {first_diagonal, off_diagonal, second_diagonal}));
                } catch (const std::runtime_error&) {
                    continue;
                }
                ++resolved;
                const slong square = off_diagonal * off_diagonal;
                const slong product = first_diagonal * second_diagonal;
                const bool expected_copositive = off_diagonal >= 0 || square <= product;
                const bool expected_strict = off_diagonal >= 0 || square < product;
                EXPECT_EQ(result.is_copositive, expected_copositive);
                EXPECT_EQ(result.is_strictly_copositive, expected_strict);
            }
        }
    }
    EXPECT_GT(resolved, 0U);
}

TEST(XxxTwoTest, UsesASecondFloatingPivotWhenThePreferredSuccessorBelongsToAKnownPath)
{
    const matrix_integer matrix = symmetric_matrix(3, {1, -2, -1, 1, 0, 1});
    EXPECT_EQ(model::xxx_two_floating_successor_for_testing(matrix, {0}, {{0, 1}}), (std::vector<size_t>{0, 2}));
}

TEST(XxxTwoTest, ExhaustsFloatingPivotsWhenEverySuccessorBelongsToKnownPaths)
{
    const matrix_integer matrix = symmetric_matrix(3, {1, -2, -1, 1, 0, 1});
    EXPECT_TRUE(model::xxx_two_floating_successor_for_testing(matrix, {0}, {{0, 1}, {0, 2}}).empty());
}

TEST(XxxTwoTest, CertifiesACompletedPathSupportWhenSatSelectsItAsASeed)
{
    const matrix_integer matrix = symmetric_matrix(2, {1, 0, 1});
    EXPECT_NO_THROW(model::xxx_two_completed_path_seed_for_testing(matrix, {0}));
}

TEST(XxxTwoTest, AddsAHalfspaceRaysIntervalWhenTheKktIntervalsMissTheSeed)
{
    const matrix_integer matrix = symmetric_matrix(5, {24067619100, -25766509860, 23359747950, 17932735800, -10193344560,
                                                       27842931900, -18758585475, 11325938400, 11679873975, 13803487425,
                                                       -9202324950, 11325938400, 7550625600, -6134883300, 5494514804});
    const auto [visited, committed, reached_kkt, seed_available] = model::xxx_buffered_path_for_testing(matrix, {4});
    EXPECT_EQ(visited, 4U);
    EXPECT_EQ(committed, 1U);
    EXPECT_EQ(reached_kkt, 1U);
    EXPECT_EQ(seed_available, 0U);
}

TEST(XxxTwoTest, BacktracksFromADeadEndAndStillClassifiesExactly)
{
    const auto result = model::classify(symmetric_matrix(4, {5622, -9558, 7425, -9558, 16250, -12623, 16250, 9806,
                                                             -12623, 16250}));
    EXPECT_TRUE(result.is_copositive);
    EXPECT_TRUE(result.is_strictly_copositive);
}

TEST(XxxTwoTest, ExactlyVerifiesANegativeWitnessBeforeTheFloatingPathReachesKkt)
{
    const matrix_integer matrix = symmetric_matrix(2, {-1, -2, 1});
    EXPECT_EQ(model::xxx_two_floating_successor_for_testing(matrix, {0}, {}), (std::vector<size_t>{0, 1}));

    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    static_cast<void>(model::xxx_buffered_path_for_testing(matrix, {0}));
    const std::string events = diagnostics::detail::load_diagnostics();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_NE(events.find("outcome=intermediate_negative"), std::string::npos);
    EXPECT_EQ(events.find("event=xxx_two_path_step"), std::string::npos);
}

TEST(XxxTwoTest, StaysExactAfterFloatingPointFalselyReportsKkt)
{
    const slong scale = slong{1} << 60;
    const auto [visited, committed, reached_kkt, seed_available] =
        model::xxx_buffered_path_for_testing(
            symmetric_matrix(3, {scale, scale - 1, scale - 2, scale, scale - 2, scale}), {0});

    EXPECT_EQ(visited, 3U);
    EXPECT_EQ(committed, 2U);
    EXPECT_EQ(reached_kkt, 1U);
    EXPECT_EQ(seed_available, 0U);
}

TEST(XxxTwoTest, AlternatesIndividualPathsFromBothEnds)
{
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    static_cast<void>(model::classify(symmetric_matrix(3, {1, 2, 2, 1, 2, 1})));
    const std::string events = diagnostics::detail::load_diagnostics();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    const size_t second_path = events.find("event=xxx_two_path path=2 outcome=verified_kkt");
    ASSERT_NE(second_path, std::string::npos);
    const size_t line_end = events.find('\n', second_path);
    const std::string event = events.substr(second_path, line_end - second_path);
    const size_t support = event.find("support=[");
    ASSERT_NE(support, std::string::npos);
    const size_t first_comma = event.find(',', support);
    EXPECT_NE(first_comma, std::string::npos);
    EXPECT_EQ(event.find(',', first_comma + 1), std::string::npos);
}

TEST(XxxTwoTest, RunsAnExactRandomPathWithSharedPathMemory)
{
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_EQ(model::xxx_two_exact_random_paths_for_testing(symmetric_matrix(3, {1, 2, 2, 1, 2, 1}), 1, 19), 1U);
    const std::string events = diagnostics::detail::load_diagnostics();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();
    EXPECT_NE(events.find("event=xxx_two_path_seed path=1"), std::string::npos);
}

TEST(XxxTwoTest, StoresExactlyVerifiedKktIntervalsInSat)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::sat_halfspace_rays_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::sat_halfspace_rays_uncovered_count(3, 2, intervals), 1U);
}

TEST(XxxTwoTest, DiagnosticsSeparateFloatingPathStepsFromTheExactKkt)
{
    const matrix_integer boundary = symmetric_matrix(2, {1, -1, 1});
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    const auto result = model::classify(boundary);
    const std::string events = diagnostics::detail::load_diagnostics();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_NE(events.find("event=xxx_two_path_step"), std::string::npos);
    EXPECT_NE(events.find("event=xxx_two_exact_kkt"), std::string::npos);
    EXPECT_NE(events.find("outcome=verified_kkt"), std::string::npos);
}

TEST(XxxTwoTest, CooperativeTimeoutInterruptsSat)
{
    request_timeout();
    EXPECT_THROW(model::sat_halfspace_rays_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
