#include <coposit/component_pipeline.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <initializer_list>
#include <vector>

using namespace coposit;

namespace {

constexpr auto copositive = model::copositivity_mode::copositive;
constexpr auto strict = model::copositivity_mode::strictly_copositive;

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

template<pre_check::detail::query requested>
matrix_scan_result full_scan(const matrix_integer& matrix)
{
    return scan_matrix(matrix, pre_check::detail::preprocessing_requirements<requested>());
}

TEST(ComponentPipelineTest, FixedPreprocessingDefaultsOn)
{
    EXPECT_TRUE(component_pipeline::options{}.preprocessing_enabled);
}

TEST(ComponentPipelineTest, MasterSwitchOffDelegatesTheOriginalMatrix)
{
    const matrix_integer matrix = symmetric_matrix(2, {1, -1, 1});
    component_pipeline::options selected;
    selected.preprocessing_enabled = false;
    progress::detail::reset();
    progress::detail::state.enabled.store(true, std::memory_order_relaxed);
    size_t calls = 0;
    EXPECT_TRUE(component_pipeline::check(matrix, copositive, selected, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(&part, &matrix);
        return true;
    }));
    const progress::snapshot snapshot = progress::detail::load();
    progress::detail::state.enabled.store(false, std::memory_order_relaxed);
    progress::detail::reset();
    EXPECT_EQ(calls, 1U);
    EXPECT_EQ(snapshot.kind, progress::metric::none);
}

TEST(ComponentPipelineTest, CompleteSmallCheckDecidesWithoutTheModel)
{
    const matrix_integer matrix = symmetric_matrix(2, {1, -2, 1});
    size_t calls = 0;
    EXPECT_FALSE(component_pipeline::check(matrix, copositive, {}, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 0U);
}

TEST(ComponentPipelineTest, OneScanCollectsBothGraphsPivotCountsAndMotzkinPattern)
{
    const matrix_integer matrix = symmetric_matrix(3, {1, -1, 1, 1, -1, 1});
    const matrix_scan_result scan = full_scan<pre_check::detail::query::combined>(matrix);

    EXPECT_EQ(scan.negative_off_diagonal_counts, (std::vector<size_t>{1, 2, 1}));
    EXPECT_EQ(scan.positive_off_diagonal_counts, (std::vector<size_t>{1, 0, 1}));
    EXPECT_TRUE(scan.negative_neighbors[0].contains(1));
    EXPECT_TRUE(scan.nonpositive_neighbors[0].contains(1));
    EXPECT_TRUE(scan.is_motzkin_straus_pattern);
}

TEST(ComponentPipelineTest, DanningerHandlesTwoChildrenAndANonStrictZeroPivot)
{
    const matrix_integer mixed = symmetric_matrix(4, {4, 1, 0, -1, 4, -1, 0, 4, 1, 4});
    const matrix_scan_result mixed_scan = full_scan<pre_check::detail::query::strict>(mixed);
    const auto pivot = danninger_precheck::detail::minimum_small_pivot(mixed, mixed_scan, strict);
    EXPECT_EQ(pivot.index, 0U);
    EXPECT_EQ(pivot.children, 2U);

    size_t children = 0;
    EXPECT_EQ(danninger_precheck::check(mixed, mixed_scan, strict, [&](const matrix_integer& child) {
        ++children;
        EXPECT_EQ(child.rows(), 3U);
        return copomatrix_precheck::outcome::accepted;
    }), copomatrix_precheck::outcome::accepted);
    EXPECT_EQ(children, 2U);

    const matrix_integer zero_pivot = symmetric_matrix(2, {0, -1, 1});
    const matrix_scan_result zero_scan = full_scan<pre_check::detail::query::copositive>(zero_pivot);
    EXPECT_EQ(danninger_precheck::check(zero_pivot, zero_scan, copositive, [&](const matrix_integer&) {
        ++children;
        return copomatrix_precheck::outcome::accepted;
    }), copomatrix_precheck::outcome::rejected);
    EXPECT_EQ(children, 2U);
}

TEST(ComponentPipelineTest, ReductionDepthAllowsChildrenAndGrandchildrenButStopsAtTheConfiguredLimit)
{
    const matrix_integer matrix = symmetric_matrix(5, {2, -5, 4, 4, 4, 14, -9, -9, -9, 6, 6, 6, 6, 6, 6});
    EXPECT_EQ(component_pipeline::detail::maximum_reduction_depth, 2U);

    const auto stopped = component_pipeline::detail::preprocess<pre_check::detail::query::strict>(matrix, 2, 2);
    EXPECT_FALSE(stopped.strict_known);

    const auto child = component_pipeline::detail::preprocess<pre_check::detail::query::strict>(matrix, 1, 2);
    ASSERT_TRUE(child.strict_known);
    EXPECT_TRUE(child.value.is_strictly_copositive);

    const auto no_reductions = component_pipeline::detail::preprocess<pre_check::detail::query::strict>(matrix, 0, 0);
    EXPECT_FALSE(no_reductions.strict_known);
}

TEST(ComponentPipelineTest, CombinedClassificationKeepsCheckingCopositivityAfterAStrictBoundary)
{
    const matrix_integer matrix = symmetric_matrix(4, {0, 1, 1, 1, 1, -2, 1, 1, 1, 1});
    size_t calls = 0;
    const auto result = component_pipeline::classify(matrix, {}, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{true, true};
    });

    EXPECT_FALSE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 0U);
}

TEST(ComponentPipelineTest, CombinedClassificationAggregatesBoundaryAndStrictComponents)
{
    const matrix_integer matrix = symmetric_matrix(4, {0, 1, 1, 1, 2, -1, 1, 2, 1, 3});
    size_t calls = 0;
    const auto result = component_pipeline::classify(matrix, {}, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{false, false};
    });

    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 0U);
}

TEST(ComponentPipelineTest, UnresolvedPreprocessingDelegatesOnlyTheOriginalMatrix)
{
    const matrix_integer matrix = symmetric_matrix(5, {1, -1, 1, 1, -1, 1, -1, 1, 1, 1, -1, 1, 1, -1, 1});
    size_t calls = 0;
    EXPECT_TRUE(component_pipeline::check(matrix, copositive, {}, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(&part, &matrix);
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

} // namespace
