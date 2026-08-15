#include <coposit/model.hpp>
#include <coposit/progress.hpp>

#include "source_trace.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
std::vector<uint64_t> ceiling_pruned_generated_masks(size_t dimension, uint64_t forbidden_trigger);
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

TEST(CeilingPrunedDickinsonTest, PreservesStrictAndCopositiveDecisions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), model::copositivity_mode::copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), model::copositivity_mode::copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), model::copositivity_mode::copositive));
}

TEST(CeilingPrunedDickinsonTest, ClassifiesBothPredicatesInOneTraversal)
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

TEST(CeilingPrunedDickinsonTest, KeepsTriggerButSkipsEveryLargerSuperset)
{
    const std::vector<uint64_t> generated = model::ceiling_pruned_generated_masks(4, 0b0101);
    EXPECT_EQ(generated.size(), 12U);
    EXPECT_NE(std::find(generated.begin(), generated.end(), 0b0101), generated.end());
    EXPECT_EQ(std::find(generated.begin(), generated.end(), 0b0111), generated.end());
    EXPECT_EQ(std::find(generated.begin(), generated.end(), 0b1101), generated.end());
    EXPECT_EQ(std::find(generated.begin(), generated.end(), 0b1111), generated.end());
}

TEST(CeilingPrunedDickinsonTest, StoresOnlyCertificatesThatReachTheCeiling)
{
    ceiling_pruned_dickinson_trace::clear();
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));

    EXPECT_EQ(std::count(ceiling_pruned_dickinson_trace::events.begin(), ceiling_pruned_dickinson_trace::events.end(),
                         ceiling_pruned_dickinson_trace::event{"discard-certificate", 1}),
              2);
    EXPECT_EQ(std::count(ceiling_pruned_dickinson_trace::events.begin(), ceiling_pruned_dickinson_trace::events.end(),
                         ceiling_pruned_dickinson_trace::event{"ceiling-certificate", 2}),
              1);
}

TEST(CeilingPrunedDickinsonTest, PublishesProgressAndCeilingCertificateDiagnostics)
{
    matrix_integer identity;
    identity.set_identity(2);

    progress::detail::reset();
    progress::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_TRUE(model::solve(identity));
    const progress::snapshot snapshot = progress::detail::load();
    progress::detail::state.enabled.store(false, std::memory_order_relaxed);
    progress::detail::reset();

    EXPECT_EQ(snapshot.kind, progress::metric::support);
    EXPECT_EQ(snapshot.nodes, 3U);
    EXPECT_EQ(snapshot.resolved, 1U);
    EXPECT_EQ(snapshot.secondary, 2U);
    EXPECT_EQ(snapshot.splits, 2U);
    EXPECT_EQ(snapshot.open, 0U);
    EXPECT_EQ(snapshot.certificate_cardinality_free_index_counts,
              (std::map<std::pair<size_t, size_t>, uint64_t>{{{1, 1}, 2}}));
}

TEST(CeilingPrunedDickinsonTest, RejectsANonCopositiveMatrix)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 2}), model::copositivity_mode::copositive));
}

TEST(CeilingPrunedDickinsonTest, KeepsPackedSupportsBeyondOneWord)
{
    matrix_integer matrix;
    matrix.set_identity(65);
    matrix(63, 64) = integer(-2);
    matrix(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(matrix));
}

} // namespace
