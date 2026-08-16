#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

using namespace coposit;

namespace coposit::model {
bool breadth_first_singular_lift_for_test(const matrix_integer& matrix, const std::vector<size_t>& root);
std::vector<uint64_t> breadth_first_generated_masks(size_t dimension, uint64_t forbidden_trigger);
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

TEST(BreadthFirstSingularLiftDickinsonTest, PreservesStrictAndCopositiveDecisions)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), model::copositivity_mode::copositive));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), model::copositivity_mode::copositive));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), model::copositivity_mode::copositive));
}

TEST(BreadthFirstSingularLiftDickinsonTest, ClassifiesBothPredicatesInOneTraversal)
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

TEST(BreadthFirstSingularLiftDickinsonTest, LiftsHighNullityAndStoresTheFinalVectorSupport)
{
    const matrix_integer matrix = symmetric_matrix(4, {
        0, 0, 1, -1,
           0, 1, -2,
              0, 0,
                 0,
    });

    breadth_first_singular_lift_dickinson_diagnostics::clear();
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_TRUE(model::breadth_first_singular_lift_for_test(matrix, {0, 1}));
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    const auto& events = breadth_first_singular_lift_dickinson_diagnostics::events;
    EXPECT_NE(std::find(events.begin(), events.end(), breadth_first_singular_lift_dickinson_diagnostics::event{"lift", 3}), events.end());
    EXPECT_NE(std::find(events.begin(), events.end(),
                        breadth_first_singular_lift_dickinson_diagnostics::event{"lift-ceiling-certificate", 2}),
              events.end());
    EXPECT_NE(snapshot.certificate_root_lifted_upper_lower_counts.find({2, 3, 4, 2}),
              snapshot.certificate_root_lifted_upper_lower_counts.end());
}

TEST(BreadthFirstSingularLiftDickinsonTest, FinishesEachLiftedCardinalityBeforeTheNext)
{
    matrix_integer matrix(5, 5);
    breadth_first_singular_lift_dickinson_diagnostics::clear();
    EXPECT_TRUE(model::breadth_first_singular_lift_for_test(matrix, {0, 1}));

    size_t previous = 0;
    for (const auto& event : breadth_first_singular_lift_dickinson_diagnostics::events) {
        if (event.name != "lift") continue;
        EXPECT_LE(previous, event.cardinality);
        previous = event.cardinality;
    }
    EXPECT_EQ(previous, 5U);
}

TEST(BreadthFirstSingularLiftDickinsonTest, ActivatesAFoundLowerSupportOnlyAfterItsOuterLayer)
{
    const std::vector<uint64_t> generated = model::breadth_first_generated_masks(4, 0b0101);
    EXPECT_EQ(generated.size(), 12U);
    EXPECT_NE(std::find(generated.begin(), generated.end(), 0b0101), generated.end());
    EXPECT_NE(std::find(generated.begin(), generated.end(), 0b0110), generated.end());
    EXPECT_EQ(std::find(generated.begin(), generated.end(), 0b0111), generated.end());
    EXPECT_EQ(std::find(generated.begin(), generated.end(), 0b1101), generated.end());
}

TEST(BreadthFirstSingularLiftDickinsonTest, DiagnosesDistinctAndDuplicateLiftedSupports)
{
    matrix_integer matrix(4, 4);

    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    EXPECT_TRUE(model::breadth_first_singular_lift_for_test(matrix, {0, 1}));
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_EQ(snapshot.secondary, 3U);
    EXPECT_EQ(snapshot.lifted_processed, 3U);
    EXPECT_EQ(snapshot.lift_duplicate_skips, 1U);
    EXPECT_EQ(snapshot.lift_covered_skips, 0U);
    EXPECT_EQ(snapshot.lift_cache_size, 4U);
    EXPECT_EQ(snapshot.lift_dimension, 0U);
    EXPECT_EQ(snapshot.lift_depth, 0U);
    EXPECT_EQ(snapshot.lift_maximum_dimension, 4U);
    EXPECT_EQ(snapshot.lift_maximum_depth, 2U);
    EXPECT_EQ(snapshot.lift_frontier_size, 0U);
    EXPECT_GT(snapshot.lift_maximum_frontier_size, 0U);
}

TEST(BreadthFirstSingularLiftDickinsonTest, KeepsPackedSupportsBeyondOneWord)
{
    matrix_integer matrix;
    matrix.set_identity(65);
    matrix(63, 64) = integer(-2);
    matrix(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(matrix));
}

} // namespace
