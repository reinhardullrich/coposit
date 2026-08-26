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

matrix_integer direct_sum(const matrix_integer& first, const matrix_integer& second)
{
    matrix_integer result(first.rows() + second.rows(), first.rows() + second.rows());
    for (size_t row = 0; row < first.rows(); ++row) {
        for (size_t column = 0; column < first.rows(); ++column)
            result(row, column) = first(row, column);
    }
    for (size_t row = 0; row < second.rows(); ++row) {
        for (size_t column = 0; column < second.rows(); ++column)
            result(first.rows() + row, first.rows() + column) = second(row, column);
    }
    return result;
}

matrix_integer unresolved_matrix()
{
    return symmetric_matrix(4, {5622, -9558, 7425, -9558, 16250, -12623, 16250, 9806, -12623, 16250});
}

template<pre_check::detail::query requested>
matrix_scan_result full_scan(const matrix_integer& matrix, support_context& context)
{
    return scan_matrix(matrix, pre_check::detail::preprocessing_requirements<requested>(), context);
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
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    size_t calls = 0;
    EXPECT_TRUE(component_pipeline::check(matrix, copositive, selected, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(&part, &matrix);
        return true;
    }));
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();
    EXPECT_EQ(calls, 1U);
    EXPECT_EQ(snapshot.kind, diagnostics::metric::none);
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
    support_context context(matrix.rows());
    const matrix_scan_result scan = full_scan<pre_check::detail::query::combined>(matrix, context);

    EXPECT_EQ(scan.negative_off_diagonal_counts, (std::vector<size_t>{1, 2, 1}));
    EXPECT_EQ(scan.positive_off_diagonal_counts, (std::vector<size_t>{1, 0, 1}));
    EXPECT_TRUE(context.contains(scan.negative_neighbors[0], 1));
    EXPECT_TRUE(context.contains(scan.nonpositive_neighbors[0], 1));
    EXPECT_TRUE(scan.is_motzkin_straus_pattern);
}

TEST(ComponentPipelineTest, DanningerHandlesTwoChildrenAndANonStrictZeroPivot)
{
    const matrix_integer mixed = symmetric_matrix(4, {4, 1, 0, -1, 4, -1, 0, 4, 1, 4});
    support_context mixed_context(mixed.rows());
    const matrix_scan_result mixed_scan = full_scan<pre_check::detail::query::strict>(mixed, mixed_context);
    const auto pivot = danninger_precheck::detail::minimum_small_pivot(mixed, mixed_scan, strict);
    EXPECT_EQ(pivot.index, 0U);
    EXPECT_EQ(pivot.children, 2U);

    size_t children = 0;
    EXPECT_EQ(danninger_precheck::check(mixed, mixed_scan, strict,
                                        [&](const matrix_integer& child) {
                                            ++children;
                                            EXPECT_EQ(child.rows(), 3U);
                                            return copomatrix_precheck::outcome::accepted;
                                        }),
              copomatrix_precheck::outcome::accepted);
    EXPECT_EQ(children, 2U);

    const matrix_integer zero_pivot = symmetric_matrix(2, {0, -1, 1});
    support_context zero_context(zero_pivot.rows());
    const matrix_scan_result zero_scan = full_scan<pre_check::detail::query::copositive>(zero_pivot, zero_context);
    EXPECT_EQ(danninger_precheck::check(zero_pivot, zero_scan, copositive,
                                        [&](const matrix_integer&) {
                                            ++children;
                                            return copomatrix_precheck::outcome::accepted;
                                        }),
              copomatrix_precheck::outcome::rejected);
    EXPECT_EQ(children, 2U);
}

TEST(ComponentPipelineTest, ReductionDepthAllowsChildrenAndGrandchildrenButStopsAtTheConfiguredLimit)
{
    const matrix_integer matrix = symmetric_matrix(5, {2, -5, 4, 4, 4, 14, -9, -9, -9, 6, 6, 6, 6, 6, 6});
    EXPECT_EQ(component_pipeline::detail::maximum_reduction_depth, 2U);

    const auto stopped = component_pipeline::detail::preprocess<pre_check::detail::query::strict>(matrix, 2, 2);
    EXPECT_FALSE(stopped.aggregate().strict_known);

    const auto child = component_pipeline::detail::preprocess<pre_check::detail::query::strict>(matrix, 1, 2);
    ASSERT_TRUE(child.aggregate().strict_known);
    EXPECT_TRUE(child.aggregate().value.is_strictly_copositive);

    const auto no_reductions = component_pipeline::detail::preprocess<pre_check::detail::query::strict>(matrix, 0, 0);
    EXPECT_FALSE(no_reductions.aggregate().strict_known);
}

TEST(ComponentPipelineTest, UnresolvedReductionKeepsItsUnchangedParentComponent)
{
    const matrix_integer matrix = symmetric_matrix(
        8, {4, -2, -3, 0, 4, 3, 1, -5, 2, 1, 0, -2, -1, -2, 3, 8, -5, -3, -3, -2, 4, 6, 0, 0, 1, 0, 4, 3, 1, -5, 4, 1, -5, 4, -3, 8});
    const auto result = component_pipeline::detail::preprocess<pre_check::detail::query::strict>(matrix, 0, 1);
    ASSERT_FALSE(result.aggregate().strict_known);
    ASSERT_EQ(result.components.size(), 1U);
    ASSERT_TRUE(result.components.front().has_matrix());
    EXPECT_EQ(&result.components.front().matrix(), &matrix);
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

TEST(ComponentPipelineTest, UnresolvedConnectedMatrixDelegatesTheOriginalMatrixWithoutCopying)
{
    const matrix_integer matrix = unresolved_matrix();
    size_t calls = 0;
    EXPECT_TRUE(component_pipeline::check(matrix, copositive, {}, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(&part, &matrix);
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

TEST(ComponentPipelineTest, NegativePartPositiveDefinitenessResolvesAConnectedIndefiniteMatrix)
{
    const matrix_integer matrix = symmetric_matrix(4, {2, -3, 10, 10, 6, -1, 10, 2, -1, 2});
    size_t calls = 0;
    const auto result = component_pipeline::classify(matrix, {}, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{false, false};
    });
    EXPECT_TRUE(result.is_copositive);
    EXPECT_TRUE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 0U);
}

TEST(ComponentPipelineTest, SingularNegativePartKeepsTheOriginalMatrixForStrictDelegation)
{
    const matrix_integer matrix = symmetric_matrix(4, {1, -1, 10, 10, 2, -1, 10, 2, -1, 1});
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    const auto result = component_pipeline::detail::preprocess<pre_check::detail::query::combined>(matrix, 0, 0);
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();
    ASSERT_EQ(result.components.size(), 1U);
    const auto& component = result.components.front();
    EXPECT_TRUE(component.partial_result.copositive_known);
    EXPECT_TRUE(component.partial_result.value.is_copositive);
    EXPECT_FALSE(component.partial_result.strict_known);
    ASSERT_TRUE(component.has_matrix());
    EXPECT_EQ(&component.matrix(), &matrix);
    EXPECT_EQ(snapshot.phase, diagnostics::preprocessing_phase::negative_part_factorization);
}

TEST(ComponentPipelineTest, DisconnectedUnresolvedComponentsDelegateOnlyTheirPrincipalMatrices)
{
    const matrix_integer hard = unresolved_matrix();
    const matrix_integer matrix = direct_sum(hard, hard);
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    size_t calls = 0;
    EXPECT_TRUE(component_pipeline::check(matrix, copositive, {}, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_NE(&part, &matrix);
        EXPECT_EQ(part.rows(), hard.rows());
        return true;
    }));
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();
    EXPECT_EQ(calls, 2U);
    EXPECT_EQ(snapshot.preprocessing_root_dimension, 8U);
    EXPECT_TRUE(snapshot.preprocessing_finished);
    EXPECT_TRUE(snapshot.preprocessing_component_split);
    EXPECT_EQ(snapshot.preprocessing_components_seen, 2U);
    EXPECT_EQ(snapshot.preprocessing_largest_component, 4U);
    EXPECT_EQ(snapshot.preprocessing_pending_components, 2U);
    EXPECT_EQ(snapshot.preprocessing_largest_pending_component, 4U);
    EXPECT_EQ(snapshot.preprocessing_model_delegations, 2U);
}

TEST(ComponentPipelineTest, ResolvedDisconnectedComponentIsNotDelegated)
{
    const matrix_integer easy = symmetric_matrix(1, {1});
    const matrix_integer hard = unresolved_matrix();
    const matrix_integer matrix = direct_sum(easy, hard);
    size_t calls = 0;
    EXPECT_TRUE(component_pipeline::check(matrix, copositive, {}, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(part.rows(), hard.rows());
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

TEST(ComponentPipelineTest, StrictModeDelegatesOnlyUnresolvedComponentsAndStopsAtFailure)
{
    const matrix_integer hard = unresolved_matrix();
    const matrix_integer matrix = direct_sum(hard, hard);
    size_t calls = 0;
    EXPECT_FALSE(component_pipeline::check(matrix, strict, {}, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(part.rows(), hard.rows());
        return false;
    }));
    EXPECT_EQ(calls, 1U);
}

TEST(ComponentPipelineTest, CombinedClassificationPreservesPartialComponentResults)
{
    const matrix_integer boundary = symmetric_matrix(1, {0});
    const matrix_integer hard = unresolved_matrix();
    const matrix_integer matrix = direct_sum(boundary, hard);
    size_t calls = 0;
    const auto result = component_pipeline::classify(matrix, {}, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(part.rows(), hard.rows());
        return model::copositivity_classification{true, true};
    });
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 1U);
}

} // namespace
