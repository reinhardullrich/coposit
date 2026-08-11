#include <coposit/connected_components.hpp>
#include <coposit/matrix_scan.hpp>

#include <gtest/gtest.h>

#include <initializer_list>
#include <stdexcept>
#include <vector>

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

TEST(ConnectedComponentsTest, FindsEveryComponentInAnExistingNegativeGraph)
{
    const matrix_integer matrix = symmetric_matrix(3, {0, 0, 0, 2, -1, 2});
    matrix_scan_requirements requirements;
    requirements.negative_graph = true;
    const matrix_scan_result scan = scan_matrix(matrix, requirements);
    size_t component_index = 0;
    std::vector<size_t> indices;
    EXPECT_EQ(connected_components::visit(scan.negative_neighbors, [&](const support& component, bool is_whole_graph) {
        EXPECT_FALSE(is_whole_graph);
        component.copy_indices_to(indices);
        if (component_index == 0) EXPECT_EQ(indices, std::vector<size_t>({0}));
        else EXPECT_EQ(indices, std::vector<size_t>({1, 2}));
        ++component_index;
        return true;
    }), 2U);
    EXPECT_EQ(component_index, 2U);

    size_t calls = 0;
    EXPECT_EQ(connected_components::visit(scan.negative_neighbors, [&](const support&, bool) {
        ++calls;
        return false;
    }), 1U);
    EXPECT_EQ(calls, 1U);
}

TEST(ConnectedComponentsTest, ReturnsOneFullIndexSetForAConnectedGraph)
{
    const matrix_integer matrix = symmetric_matrix(3, {2, -1, 0, 2, -1, 2});
    matrix_scan_requirements requirements;
    requirements.negative_graph = true;
    const matrix_scan_result scan = scan_matrix(matrix, requirements);
    std::vector<size_t> indices;

    EXPECT_EQ(connected_components::visit(scan.negative_neighbors, [&](const support& component, bool is_whole_graph) {
        EXPECT_TRUE(is_whole_graph);
        component.copy_indices_to(indices);
        return true;
    }), 1U);
    EXPECT_EQ(indices, std::vector<size_t>({0, 1, 2}));
}

TEST(ConnectedComponentsTest, HasNoFixedDimensionLimitAndValidatesItsInput)
{
    matrix_integer wide(129, 129);
    for (size_t index = 0; index < 129; ++index) wide(index, index) = integer(3);
    wide(63, 64) = wide(64, 63) = integer(-4);
    wide(64, 128) = wide(128, 64) = integer(-4);
    matrix_scan_requirements requirements;
    requirements.negative_graph = true;
    const matrix_scan_result scan = scan_matrix(wide, requirements);
    size_t size_three = 0;
    std::vector<size_t> indices;
    EXPECT_EQ(connected_components::visit(scan.negative_neighbors, [&](const support& component, bool is_whole_graph) {
        EXPECT_FALSE(is_whole_graph);
        component.copy_indices_to(indices);
        size_three += indices.size() == 3;
        return true;
    }), 127U);
    EXPECT_EQ(size_three, 1U);

    EXPECT_THROW(scan_matrix(matrix_integer(), requirements), std::invalid_argument);
    EXPECT_THROW(scan_matrix(matrix_integer(2, 3), requirements), std::invalid_argument);

    matrix_integer asymmetric = symmetric_matrix(2, {1, -1, 1});
    asymmetric(1, 0) = integer(0);
    EXPECT_THROW(scan_matrix(asymmetric, requirements), std::invalid_argument);
}

} // namespace
