#include <coposit/model.hpp>

#include "source_trace.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
std::vector<uint64_t> maximal_z_block_masks(const matrix_integer& matrix);
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

matrix_integer constant_off_diagonal(size_t dimension, slong diagonal, slong off_diagonal)
{
    matrix_integer matrix(dimension, dimension);
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = 0; column < dimension; ++column) {
            matrix(row, column) = integer(row == column ? diagonal : off_diagonal);
        }
    }
    return matrix;
}

std::vector<uint64_t> brute_maximal_cliques(size_t dimension, uint64_t graph)
{
    std::vector<uint64_t> result;
    const auto has_edge = [&](size_t left, size_t right) {
        if (left > right) std::swap(left, right);
        const size_t edge = left * (2 * dimension - left - 1) / 2 + right - left - 1;
        return (graph & (uint64_t{1} << edge)) != 0;
    };

    for (uint64_t candidate = 1; candidate < (uint64_t{1} << dimension); ++candidate) {
        if ((candidate & (candidate - 1)) == 0) continue;
        bool clique = true;
        for (size_t left = 0; left < dimension; ++left)
            for (size_t right = left + 1; right < dimension; ++right)
                if ((candidate & (uint64_t{1} << left)) != 0 && (candidate & (uint64_t{1} << right)) != 0)
                    clique &= has_edge(left, right);
        if (!clique) continue;

        bool maximal = true;
        for (size_t outside = 0; outside < dimension; ++outside) {
            if ((candidate & (uint64_t{1} << outside)) != 0) continue;
            bool extends = true;
            for (size_t inside = 0; inside < dimension; ++inside)
                if ((candidate & (uint64_t{1} << inside)) != 0) extends &= has_edge(outside, inside);
            maximal &= !extends;
        }
        if (maximal) result.push_back(candidate);
    }
    return result;
}

TEST(DickinsonZedTest, EnumeratesEveryMaximalBlockForEveryFiveVertexSignGraph)
{
    constexpr size_t dimension = 5;
    constexpr size_t edge_count = dimension * (dimension - 1) / 2;
    for (uint64_t graph = 0; graph < (uint64_t{1} << edge_count); ++graph) {
        matrix_integer matrix(dimension, dimension);
        for (size_t row = 0; row < dimension; ++row) {
            matrix(row, row) = integer(10);
            for (size_t column = row + 1; column < dimension; ++column) {
                const size_t edge = row * (2 * dimension - row - 1) / 2 + column - row - 1;
                matrix(row, column) = integer((graph & (uint64_t{1} << edge)) != 0 ? -1 : 1);
                matrix(column, row) = matrix(row, column);
            }
        }
        EXPECT_EQ(model::maximal_z_block_masks(matrix), brute_maximal_cliques(dimension, graph));
    }
}

TEST(DickinsonZedTest, DistinguishesStrictMatricesZerosAndNegativeWitnesses)
{
    EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
    EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
    EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(DickinsonZedTest, HandlesSingularCertificatesExactly)
{
    EXPECT_FALSE(model::solve(constant_off_diagonal(4, 3, -1)));
    EXPECT_TRUE(model::solve(constant_off_diagonal(4, 1, 1)));

    constexpr slong vector[] = {1, -1, 1, -1};
    matrix_integer mixed_sign_nullspace(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            mixed_sign_nullspace(row, column) = integer((row == column ? 4 : 0) - vector[row] * vector[column]);
        }
    }
    EXPECT_TRUE(model::solve(mixed_sign_nullspace));
}

TEST(DickinsonZedTest, CertifiesAWholePositiveDefiniteZMatrixBeforeDickinson)
{
    matrix_integer identity;
    identity.set_identity(4);
    dickinson_zed_trace::clear();
    EXPECT_TRUE(model::solve(identity));

    ASSERT_EQ(dickinson_zed_trace::events.size(), 20U);
    for (size_t i = 0; i < 4; ++i)
        EXPECT_EQ(dickinson_zed_trace::events[i], (dickinson_zed_trace::event{"z-component", 1}));
    EXPECT_EQ(dickinson_zed_trace::events[4], (dickinson_zed_trace::event{"z-covered", 4}));
    for (size_t i = 5; i < dickinson_zed_trace::events.size(); ++i)
        EXPECT_EQ(dickinson_zed_trace::events[i].name, "covered");
}

TEST(DickinsonZedTest, FindsEveryMaximalZBlockWhenExtensionsConflict)
{
    const matrix_integer matrix = symmetric_matrix(4, {10, -1, -1, -1, 10, -1, -1, 10, 1, 10});
    dickinson_zed_trace::clear();
    EXPECT_TRUE(model::solve(matrix));

    size_t maximal_blocks = 0;
    for (const auto& event : dickinson_zed_trace::events)
        maximal_blocks += event == dickinson_zed_trace::event{"z-covered", 3};
    EXPECT_EQ(maximal_blocks, 2U);
}

TEST(DickinsonZedTest, RejectsASingularPositiveSemidefiniteZBlockImmediately)
{
    dickinson_zed_trace::clear();
    EXPECT_FALSE(model::solve(constant_off_diagonal(4, 3, -1)));
    ASSERT_EQ(dickinson_zed_trace::events.size(), 2U);
    EXPECT_EQ(dickinson_zed_trace::events[0], (dickinson_zed_trace::event{"z-component", 4}));
    EXPECT_EQ(dickinson_zed_trace::events[1], (dickinson_zed_trace::event{"z-reject", 4}));
}

TEST(DickinsonZedTest, PreservesArbitraryPrecisionScaling)
{
    integer scale;
    ASSERT_EQ(fmpz_set_str(scale.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer strict = constant_off_diagonal(4, 5, -1);
    fmpz_mat_scalar_mul_fmpz(strict.native_handle(), strict.native_handle(), scale.native_handle());
    EXPECT_TRUE(model::solve(strict));

    matrix_integer boundary = constant_off_diagonal(4, 3, -1);
    fmpz_mat_scalar_mul_fmpz(boundary.native_handle(), boundary.native_handle(), scale.native_handle());
    EXPECT_FALSE(model::solve(boundary));
}

TEST(DickinsonZedTest, ClassifiesBoundaryStressMatrix9161)
{
    EXPECT_FALSE(model::solve(symmetric_matrix(5, {1, -1, 1, 2, -3, 2, -3, -3, 4, 5, 6, -4, 5, -8, 16})));
}

TEST(DickinsonZedTest, UsesPackedCoverageBeyondOneWord)
{
    matrix_integer not_strictly_copositive;
    not_strictly_copositive.set_identity(65);
    not_strictly_copositive(63, 64) = integer(-2);
    not_strictly_copositive(64, 63) = integer(-2);
    EXPECT_FALSE(model::solve(not_strictly_copositive));
}

TEST(DickinsonZedTest, RejectsNonStrictMode)
{
    EXPECT_THROW(model::solve(symmetric_matrix(1, {1}), model::copositivity_mode::copositive), std::invalid_argument);
}

} // namespace
