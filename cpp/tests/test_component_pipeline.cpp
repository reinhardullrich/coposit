#include <coposit/component_pipeline.hpp>

#include <gtest/gtest.h>

#include <initializer_list>
#include <stdexcept>
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

TEST(ComponentPipelineTest, DefaultsBothStagesAndEveryPreCheckOn)
{
    component_pipeline::options selected;
    EXPECT_TRUE(selected.pre_checks_enabled);
    EXPECT_TRUE(selected.connected_components);
    EXPECT_TRUE(selected.pre_checks.small_dimension);
    EXPECT_TRUE(selected.pre_checks.principal_submatrices);
    EXPECT_TRUE(selected.pre_checks.nonnegative_off_diagonal);
    EXPECT_TRUE(selected.pre_checks.negative_part_diagonal_dominance);
    EXPECT_TRUE(selected.pre_checks.all_ones);
    EXPECT_TRUE(selected.pre_checks.frank_wolfe);
    EXPECT_TRUE(selected.pre_checks.positive_definiteness);

}

TEST(ComponentPipelineTest, BothStagesCanBeDisabled)
{
    component_pipeline::options selected;
    selected.pre_checks_enabled = false;
    selected.connected_components = false;
    matrix_integer matrix;
    size_t calls = 0;
    EXPECT_TRUE(component_pipeline::check(matrix, copositive, selected, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(&part, &matrix);
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

TEST(ComponentPipelineTest, ComponentsAndPreChecksCanBeSelectedIndependently)
{
    const matrix_integer matrix = symmetric_matrix(3, {1, -2, 10, 1, 10, 10});
    component_pipeline::options selected;
    selected.connected_components = false;
    selected.pre_checks = pre_check::options::none();
    selected.pre_checks.all_ones = true;
    size_t calls = 0;
    EXPECT_TRUE(component_pipeline::check(matrix, copositive, selected, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(&part, &matrix);
        return true;
    }));
    EXPECT_EQ(calls, 1U);

    selected.connected_components = true;
    EXPECT_FALSE(component_pipeline::check(matrix, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);

    selected.pre_checks_enabled = false;
    std::vector<size_t> dimensions;
    EXPECT_TRUE(component_pipeline::check(matrix, copositive, selected, [&](const matrix_integer& part) {
        ++calls;
        dimensions.push_back(part.rows());
        return true;
    }));
    EXPECT_EQ(calls, 3U);
    EXPECT_EQ(dimensions, std::vector<size_t>({2, 1}));
}

TEST(ComponentPipelineTest, CombinedClassificationKeepsCheckingCopositivityAfterAStrictBoundary)
{
    const matrix_integer matrix = symmetric_matrix(4, {0, 1, 1, 1, 1, -2, 1, 1, 1, 1});
    component_pipeline::options selected;
    selected.pre_checks = pre_check::options::none();
    selected.pre_checks.small_dimension = true;
    size_t calls = 0;
    const auto result = component_pipeline::classify(matrix, selected, [&](const matrix_integer&) {
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
    component_pipeline::options selected;
    selected.pre_checks = pre_check::options::none();
    selected.pre_checks.small_dimension = true;
    size_t calls = 0;
    const auto result = component_pipeline::classify(matrix, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{false, false};
    });

    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 0U);
}

TEST(ComponentPipelineTest, ConnectedInputReusesTheOriginalMatrix)
{
    const matrix_integer matrix = symmetric_matrix(3, {2, -1, 0, 2, -1, 2});
    component_pipeline::options selected;
    selected.pre_checks_enabled = false;
    size_t calls = 0;
    EXPECT_TRUE(component_pipeline::check(matrix, strict, selected, [&](const matrix_integer& part) {
        ++calls;
        EXPECT_EQ(&part, &matrix);
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

} // namespace
