#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <map>

using namespace coposit;

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

TEST(ClingoSatDickinsonTest, PreservesStrictDickinsonDecisions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(ClingoSatDickinsonTest, DistinguishesNonStrictFromStrictCopositivity)
{
    const auto copositive = model::copositivity_mode::copositive;
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
}

TEST(ClingoSatDickinsonTest, ClassifiesBothPredicatesInOneTraversal)
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

TEST(ClingoSatDickinsonTest, KeepsSingletonCertificatesAcrossLaterCardinalities)
{
    matrix_integer identity;
    identity.set_identity(4);
    clingo_sat_dickinson_diagnostics::clear();
    EXPECT_TRUE(model::solve(identity));

    ASSERT_EQ(clingo_sat_dickinson_diagnostics::events.size(), 4U);
    for (const auto& event : clingo_sat_dickinson_diagnostics::events)
        EXPECT_EQ(event, (clingo_sat_dickinson_diagnostics::event{"process", 1}));
}

TEST(ClingoSatDickinsonTest, KeepsPartialSingletonIntervalsAcrossCardinalityCalls)
{
    const matrix_integer matrix = symmetric_matrix(3, {1, 1, -1, 1, -1, 1});
    clingo_sat_dickinson_diagnostics::clear();
    const auto classification = model::classify(matrix);

    EXPECT_TRUE(classification.is_copositive);
    EXPECT_FALSE(classification.is_strictly_copositive);
    std::map<size_t, size_t> processed;
    for (const auto& event : clingo_sat_dickinson_diagnostics::events)
        if (event.name == "process") ++processed[event.cardinality];
    EXPECT_EQ(processed, (std::map<size_t, size_t>{{1, 3}, {2, 2}}));
}

TEST(ClingoSatDickinsonTest, ExhaustsEverySupportWhenAllDickinsonIntervalsAreSingletons)
{
    constexpr size_t dimension = 8;
    matrix_integer matrix(dimension, dimension);
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = 0; column < dimension; ++column)
            matrix(row, column) = integer(row == column ? static_cast<slong>(dimension) : -1);
    }

    clingo_sat_dickinson_diagnostics::clear();
    EXPECT_TRUE(model::solve(matrix));

    std::map<size_t, size_t> processed;
    for (const auto& event : clingo_sat_dickinson_diagnostics::events)
        if (event.name == "process") ++processed[event.cardinality];
    EXPECT_EQ(processed, (std::map<size_t, size_t>{{1, 8}, {2, 28}, {3, 56}, {4, 70}, {5, 56}, {6, 28}, {7, 8}, {8, 1}}));
}

TEST(ClingoSatDickinsonTest, PublishesCertificateFreeIndexAndUpperSizeDistributionOnlyWithDiagnostics)
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

TEST(ClingoSatDickinsonTest, AcceptsABoundaryMatrixInNonStrictMode)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -2, 4}), model::copositivity_mode::copositive));
}

TEST(ClingoSatDickinsonTest, HandlesSupportAtomsBeyondOneWord)
{
    matrix_integer matrix;
    matrix.set_identity(65);
    matrix(63, 64) = integer(-2);
    matrix(64, 63) = integer(-2);

    EXPECT_FALSE(model::solve(matrix));
}

} // namespace
