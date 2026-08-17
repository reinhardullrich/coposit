#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
bool wide_certificate_sat_uses_full_interval(size_t dimension, size_t cardinality, size_t free_indices);
size_t wide_certificate_sat_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
size_t wide_certificate_sat_interval_count(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
size_t wide_certificate_sat_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask);
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

class WideCertificateSatDickinsonTest : public ::testing::Test {
protected:
    void SetUp() override { model::configure("100"); }
};

TEST(WideCertificateSatDickinsonPolicyTest, UsesConfiguredPercentageOfRemainingWidth)
{
    model::configure("75");
    EXPECT_FALSE(model::wide_certificate_sat_uses_full_interval(50, 2, 36));
    EXPECT_TRUE(model::wide_certificate_sat_uses_full_interval(50, 2, 37));

    model::configure("90");
    EXPECT_FALSE(model::wide_certificate_sat_uses_full_interval(52, 2, 45));
    EXPECT_TRUE(model::wide_certificate_sat_uses_full_interval(52, 2, 46));

    model::configure("95");
    EXPECT_FALSE(model::wide_certificate_sat_uses_full_interval(42, 2, 38));
    EXPECT_TRUE(model::wide_certificate_sat_uses_full_interval(42, 2, 39));
}

TEST(WideCertificateSatDickinsonPolicyTest, RejectsInvalidPercentages)
{
    EXPECT_THROW(model::configure(""), std::invalid_argument);
    EXPECT_THROW(model::configure("75%"), std::invalid_argument);
    EXPECT_THROW(model::configure("101"), std::invalid_argument);
    model::configure("50");
}

TEST(WideCertificateSatDickinsonPolicyTest, StoresOnlyExactSupportClausesForFullSupportVectorsAtOneHundredPercent)
{
    model::configure("100");
    wide_certificate_sat_dickinson_diagnostics::clear();
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, 0, 1})));
    EXPECT_TRUE(std::none_of(
        wide_certificate_sat_dickinson_diagnostics::events.begin(),
        wide_certificate_sat_dickinson_diagnostics::events.end(),
        [](const auto& event) { return event.name == "wide-certificate"; }));
    EXPECT_TRUE(std::any_of(
        wide_certificate_sat_dickinson_diagnostics::events.begin(),
        wide_certificate_sat_dickinson_diagnostics::events.end(),
        [](const auto& event) { return event.name == "narrow-certificate"; }));
}

TEST_F(WideCertificateSatDickinsonTest, PreservesStrictDickinsonDecisions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST_F(WideCertificateSatDickinsonTest, DistinguishesNonStrictFromStrictCopositivity)
{
    const auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
}

TEST_F(WideCertificateSatDickinsonTest, ClassifiesBothPredicatesInOneTraversal)
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

TEST_F(WideCertificateSatDickinsonTest, SubtractsTheUnionOfBoundedIntervals)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::wide_certificate_sat_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::wide_certificate_sat_uncovered_count(3, 2, intervals), 1U);
}

TEST_F(WideCertificateSatDickinsonTest, StoresOneBlockingClausePerRetainedInterval)
{
    EXPECT_EQ(model::wide_certificate_sat_interval_count(8, 0b001, 0b111), 1U);
}

TEST_F(WideCertificateSatDickinsonTest, AddsTheExistingCardinalityOutputOnlyToExpiringIntervals)
{
    EXPECT_EQ(model::wide_certificate_sat_interval_clause_size(8, 0b00000001, 0b00000111), 7U);
    EXPECT_EQ(model::wide_certificate_sat_interval_clause_size(8, 0b00000001, 0b11111111), 1U);
}

TEST_F(WideCertificateSatDickinsonTest, PublishesGeneratedCertificateGeometryWithDiagnostics)
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
              (std::map<std::tuple<size_t, size_t, size_t>, uint64_t>{{{1, 1, 2}, 2}, {{2, 0, 2}, 1}}));
}

TEST_F(WideCertificateSatDickinsonTest, KeepsPackedSupportsBeyondOneWord)
{
    matrix_integer matrix;
    matrix.set_identity(65);
    matrix(63, 64) = integer(-2);
    matrix(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(matrix));
}

TEST_F(WideCertificateSatDickinsonTest, CooperativeTimeoutInterruptsSatSolving)
{
    request_timeout();
    EXPECT_THROW(model::wide_certificate_sat_uncovered_count(15, 7, {}), timeout_requested);
    reset_timeout();
}

} // namespace
