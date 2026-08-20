#include <coposit/pre_check.hpp>
#include <coposit/open_mcs.hpp>
#include <coposit/small_copositivity.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>

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

matrix_integer motzkin_straus_matrix(size_t dimension, slong diagonal, std::uint32_t edges)
{
    matrix_integer matrix(dimension, dimension);
    std::uint32_t edge = 1;
    for (size_t row = 0; row < dimension; ++row) {
        matrix(row, row) = integer(diagonal);
        for (size_t column = row + 1; column < dimension; ++column) {
            matrix(row, column) = integer((edges & edge) != 0 ? -1 : diagonal);
            matrix(column, row) = matrix(row, column);
            edge <<= 1;
        }
    }
    return matrix;
}

size_t maximum_clique(size_t dimension, std::uint32_t edges)
{
    size_t best = 1;
    for (std::uint32_t vertices = 1; vertices < (std::uint32_t{1} << dimension); ++vertices) {
        bool clique = true;
        size_t cardinality = 0;
        std::uint32_t edge = 1;
        for (size_t row = 0; row < dimension; ++row) {
            if ((vertices & (std::uint32_t{1} << row)) != 0) ++cardinality;
            for (size_t column = row + 1; column < dimension; ++column) {
                if ((vertices & (std::uint32_t{1} << row)) != 0 && (vertices & (std::uint32_t{1} << column)) != 0
                    && (edges & edge) == 0)
                    clique = false;
                edge <<= 1;
            }
        }
        if (clique) best = std::max(best, cardinality);
    }
    return best;
}

std::vector<support> graph_adjacency(size_t dimension, std::uint32_t edges)
{
    std::vector<support> adjacency;
    adjacency.reserve(dimension);
    for (size_t vertex = 0; vertex < dimension; ++vertex) adjacency.emplace_back(dimension);

    std::uint32_t edge = 1;
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = row + 1; column < dimension; ++column) {
            if ((edges & edge) != 0) {
                adjacency[row].set(column);
                adjacency[column].set(row);
            }
            edge <<= 1;
        }
    }
    return adjacency;
}

TEST(PreCheckTest, DefaultsToOnAndCanBeSwitchedFullyOff)
{
    const pre_check::options defaults;
    EXPECT_EQ(defaults.principal_submatrices_up_to, 3U);
    EXPECT_TRUE(defaults.z_matrix);
    EXPECT_FALSE(pre_check::options::none().z_matrix);

    size_t calls = 0;
    EXPECT_TRUE(pre_check::check(matrix_integer(), copositive, pre_check::options::none(), [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

TEST(PreCheckTest, ZMatrixCheckIsNegativeOnlyAndModeAware)
{
    pre_check::options selected = pre_check::options::none();
    selected.z_matrix = true;
    size_t calls = 0;

    const matrix_integer positive_definite_block = symmetric_matrix(4, {2, -1, 3, 3, 2, 3, 3, 2, 3, 2});
    EXPECT_TRUE(pre_check::check(positive_definite_block, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);

    const matrix_integer singular_block = symmetric_matrix(4, {1, -1, 2, 2, 1, 2, 2, 1, 2, 1});
    EXPECT_FALSE(pre_check::check(singular_block, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);
    EXPECT_TRUE(pre_check::check(singular_block, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 2U);

    const auto boundary = pre_check::classify(singular_block, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{true, true};
    });
    EXPECT_TRUE(boundary.is_copositive);
    EXPECT_FALSE(boundary.is_strictly_copositive);
    EXPECT_EQ(calls, 3U);

    const matrix_integer indefinite_block = symmetric_matrix(4, {1, -2, 2, 2, 1, 2, 2, 1, 2, 1});
    EXPECT_FALSE(pre_check::check(indefinite_block, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    const auto rejected = pre_check::classify(indefinite_block, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{true, true};
    });
    EXPECT_FALSE(rejected.is_copositive);
    EXPECT_FALSE(rejected.is_strictly_copositive);
    EXPECT_EQ(calls, 3U);
}

TEST(PreCheckTest, ZMatrixCheckIncludesSingletonBlocksAndMotzkinStrausMatricesUseTheirExactSolver)
{
    pre_check::options selected = pre_check::options::none();
    selected.z_matrix = true;
    size_t calls = 0;

    const matrix_integer negative_singleton = symmetric_matrix(4, {-1, 2, 2, 2, 1, 2, 2, 1, 2, 1});
    EXPECT_FALSE(pre_check::check(negative_singleton, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 0U);

    const matrix_integer motzkin_straus = symmetric_matrix(4, {1, -1, 1, 1, 1, 1, 1, 1, 1, 1});
    EXPECT_FALSE(pre_check::check(motzkin_straus, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 0U);
}

TEST(PreCheckTest, MotzkinStrausBranchAndBoundMatchesEveryOrderFiveGraph)
{
    pre_check::options selected = pre_check::options::none();
    selected.z_matrix = true;
    constexpr size_t dimension = 5;
    constexpr std::uint32_t graph_count = std::uint32_t{1} << (dimension * (dimension - 1) / 2);

    for (std::uint32_t edges = 1; edges < graph_count; ++edges) {
        const size_t clique = maximum_clique(dimension, edges);
        for (slong diagonal = 1; diagonal <= 4; ++diagonal) {
            size_t calls = 0;
            const auto result = pre_check::classify(motzkin_straus_matrix(dimension, diagonal, edges), selected,
                                                    [&](const matrix_integer&) {
                                                        ++calls;
                                                        return model::copositivity_classification{false, false};
                                                    });
            EXPECT_EQ(result.is_copositive, clique <= static_cast<size_t>(diagonal + 1));
            EXPECT_EQ(result.is_strictly_copositive, clique < static_cast<size_t>(diagonal + 1));
            EXPECT_EQ(calls, 0U);
        }
    }
}

TEST(PreCheckTest, OpenMcsMatchesEveryOrderSixGraph)
{
    constexpr size_t dimension = 6;
    constexpr std::uint32_t graph_count = std::uint32_t{1} << (dimension * (dimension - 1) / 2);
    for (std::uint32_t edges = 0; edges < graph_count; ++edges) {
        const auto adjacency = graph_adjacency(dimension, edges);
        open_mcs::maximum_clique_search search(adjacency);
        const auto result = search.run([](size_t) { return false; });
        ASSERT_TRUE(result.complete) << "graph=" << edges;
        ASSERT_EQ(result.best, maximum_clique(dimension, edges)) << "graph=" << edges;
    }
}

TEST(PreCheckTest, OpenMcsThresholdStopReturnsAProvedCliqueWithoutClaimingCompleteness)
{
    std::vector<support> cycle;
    for (size_t vertex = 0; vertex < 5; ++vertex) cycle.emplace_back(5);
    for (size_t vertex = 0; vertex < 5; ++vertex) {
        const size_t next = (vertex + 1) % 5;
        cycle[vertex].set(next);
        cycle[next].set(vertex);
    }

    open_mcs::maximum_clique_search search(cycle);
    const auto result = search.run([](size_t best) { return best >= 2; });
    EXPECT_EQ(result.best, 2U);
    EXPECT_FALSE(result.complete);
}

TEST(PreCheckTest, MotzkinStrausBranchAndBoundUsesExactNonintegralThresholdsAndSupportsMoreThanSixtyFourIndices)
{
    pre_check::options selected = pre_check::options::none();
    selected.z_matrix = true;

    const auto nonintegral = pre_check::classify(constant_off_diagonal(3, 3, -2), selected, [&](const matrix_integer&) {
        return model::copositivity_classification{true, true};
    });
    EXPECT_FALSE(nonintegral.is_copositive);
    EXPECT_FALSE(nonintegral.is_strictly_copositive);

    const auto boundary = pre_check::classify(constant_off_diagonal(65, 64, -1), selected, [&](const matrix_integer&) {
        return model::copositivity_classification{false, false};
    });
    EXPECT_TRUE(boundary.is_copositive);
    EXPECT_FALSE(boundary.is_strictly_copositive);
}

TEST(PreCheckTest, SmallDimensionMakesACompleteModeDependentDecision)
{
    pre_check::options selected = pre_check::options::none();
    selected.small_dimension = true;
    const matrix_integer boundary = symmetric_matrix(2, {1, -1, 1});
    size_t calls = 0;
    const auto final_algorithm = [&](const matrix_integer&) {
        ++calls;
        return false;
    };

    EXPECT_FALSE(pre_check::check(boundary, strict, selected, final_algorithm));
    EXPECT_TRUE(pre_check::check(boundary, copositive, selected, final_algorithm));
    EXPECT_EQ(calls, 0U);
}

TEST(PreCheckTest, LargerMatrixPrincipalChecksOnlyReject)
{
    pre_check::options selected = pre_check::options::none();
    selected.principal_submatrices = true;
    selected.principal_submatrices_up_to = 3;

    // The leading order-three face is positive semidefinite with a zero at (1,1,1); all of its order-two faces are strict.
    const matrix_integer boundary = symmetric_matrix(4, {2, -1, -1, 0, 2, -1, 0, 2, 0, 2});
    size_t calls = 0;
    EXPECT_FALSE(pre_check::check(boundary, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 0U);
    EXPECT_TRUE(pre_check::check(boundary, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);

    // Every order-two face still passes, but the leading triple has a negative all-ones value.
    const matrix_integer negative = symmetric_matrix(4, {4, -3, -3, 0, 4, -3, 0, 4, 0, 4});
    EXPECT_FALSE(pre_check::check(negative, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_FALSE(pre_check::check(negative, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);

    // This failing triple has a two-edge negative path rather than a negative triangle.
    const matrix_integer negative_path = symmetric_matrix(4, {4, -3, 0, 0, 4, -3, 0, 4, 0, 4});
    EXPECT_FALSE(pre_check::check(negative_path, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);

    matrix_integer identity;
    identity.set_identity(4);
    EXPECT_FALSE(pre_check::check(identity, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_EQ(calls, 2U);
}

TEST(PreCheckTest, PrincipalSubmatrixCutoffSelectsCardinalityOneTwoOrThree)
{
    pre_check::options selected = pre_check::options::none();
    size_t calls = 0;
    const auto final_algorithm = [&](const matrix_integer&) {
        ++calls;
        return true;
    };

    const matrix_integer bad_diagonal = symmetric_matrix(4, {-1, 0, 0, 0, 1, 0, 0, 1, 0, 1});
    selected.principal_submatrices = true;
    selected.principal_submatrices_up_to = 1;
    EXPECT_FALSE(pre_check::check(bad_diagonal, copositive, selected, final_algorithm));
    EXPECT_EQ(calls, 0U);

    const matrix_integer bad_pair = symmetric_matrix(4, {1, -2, 0, 0, 1, 0, 0, 1, 0, 1});
    EXPECT_TRUE(pre_check::check(bad_pair, copositive, selected, final_algorithm));
    EXPECT_EQ(calls, 1U);
    selected.principal_submatrices_up_to = 2;
    EXPECT_FALSE(pre_check::check(bad_pair, copositive, selected, final_algorithm));
    EXPECT_EQ(calls, 1U);

    const matrix_integer bad_triple = symmetric_matrix(4, {4, -3, 0, 0, 4, -3, 0, 4, 0, 4});
    EXPECT_TRUE(pre_check::check(bad_triple, copositive, selected, final_algorithm));
    EXPECT_EQ(calls, 2U);
    selected.principal_submatrices_up_to = 3;
    EXPECT_FALSE(pre_check::check(bad_triple, copositive, selected, final_algorithm));
    EXPECT_EQ(calls, 2U);
}

TEST(PreCheckTest, WholeMatrixRulesShareOnlyTheModeDependentEqualityBoundary)
{
    pre_check::options selected = pre_check::options::none();
    size_t calls = 0;

    const matrix_integer zero_diagonal = symmetric_matrix(4, {0, 0, 0, 0, 1, 0, 0, 1, 0, 1});
    selected.nonnegative_off_diagonal = true;
    EXPECT_FALSE(pre_check::check(zero_diagonal, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_TRUE(pre_check::check(zero_diagonal, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_EQ(calls, 0U);

    // Every negative-part row sum and the all-ones quadratic value are exactly zero.
    const matrix_integer equality = symmetric_matrix(4, {2, -1, 0, -1, 2, -1, 0, 2, -1, 2});
    selected = pre_check::options::none();
    selected.negative_part_diagonal_dominance = true;
    EXPECT_FALSE(pre_check::check(equality, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_TRUE(pre_check::check(equality, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_EQ(calls, 1U);

    selected = pre_check::options::none();
    selected.all_ones = true;
    EXPECT_FALSE(pre_check::check(equality, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_TRUE(pre_check::check(equality, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 2U);
}

TEST(PreCheckTest, FloatingFrankWolfeFindsAnExactlyVerifiedIterativeWitnessAndUsesTheModeBoundary)
{
    pre_check::options selected = pre_check::options::none();
    selected.frank_wolfe = true;
    size_t calls = 0;

    const matrix_integer iterative_witness = symmetric_matrix(4, {11, -9, 5, -12, 10, -8, 3, 25, -10, 19});
    EXPECT_FALSE(pre_check::check(iterative_witness, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_FALSE(pre_check::check(iterative_witness, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 0U);

    const matrix_integer zero_at_centre = constant_off_diagonal(4, 3, -1);
    EXPECT_FALSE(pre_check::check(zero_at_centre, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_TRUE(pre_check::check(zero_at_centre, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);

    matrix_integer identity;
    identity.set_identity(4);
    EXPECT_FALSE(pre_check::check(identity, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_EQ(calls, 2U);
}

TEST(PreCheckTest, FloatingFrankWolfeNeverUsesARoundedObjectiveAsACertificate)
{
    pre_check::options selected = pre_check::options::none();
    selected.frank_wolfe = true;

    // A = 2^2000 vv^T + I for v = (1,-1,1,-1). After global scaling, the identity contribution underflows in double and the
    // centre appears to have value zero. The exact quadratic value is positive, so the pre-check must delegate.
    integer large(1);
    fmpz_mul_2exp(large.native_handle(), large.native_handle(), 2000);
    const int signs[] = {1, -1, 1, -1};
    const integer one(1);
    matrix_integer rounded_zero(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            rounded_zero(row, column) = large;
            if (signs[row] != signs[column]) rounded_zero(row, column).negate();
            if (row == column) rounded_zero(row, column) += one;
        }
    }

    size_t calls = 0;
    EXPECT_TRUE(pre_check::check(rounded_zero, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

TEST(PreCheckTest, FloatingFrankWolfeHandlesTheFormerExactDenominatorGrowthCase)
{
    pre_check::options selected = pre_check::options::none();
    selected.frank_wolfe = true;

    matrix_integer lift(22, 22);
    for (size_t row = 0; row < 22; ++row) {
        for (size_t column = 0; column < 22; ++column) {
            slong value = 6;
            if (row == 0 && column == 0) value = 2;
            else if ((row == 0 && column == 1) || (row == 1 && column == 0)) value = -5;
            else if (row == 0 || column == 0) value = 4;
            else if (row == 1 && column == 1) value = 14;
            else if (row == 1 || column == 1) value = -9;
            lift(row, column) = integer(value);
        }
    }

    size_t calls = 0;
    EXPECT_TRUE(pre_check::check(lift, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

TEST(PreCheckTest, HeuristicKktSearchReportsOnlyExactlyVerifiedSigns)
{
    const auto negative = pre_check::detail::run_heuristic_kkt_search(symmetric_matrix(2, {1, -2, 1}));
    ASSERT_TRUE(negative.sign);
    EXPECT_EQ(*negative.sign, -1);
    EXPECT_TRUE(negative.reached_kkt);

    const auto zero = pre_check::detail::run_heuristic_kkt_search(symmetric_matrix(2, {1, -1, 1}));
    ASSERT_TRUE(zero.sign);
    EXPECT_EQ(*zero.sign, 0);
    EXPECT_TRUE(zero.reached_kkt);
}

TEST(PreCheckTest, HeuristicKktSearchContinuesExactlyAfterFloatingDisagreement)
{
    const matrix_integer matrix = symmetric_matrix(
        3, {4503599627370522, 4503599627370516, 4503599627370514, 4503599627370485, 4503599627370513,
            4503599627370493});
    const auto result = pre_check::detail::run_heuristic_kkt_search(matrix);
    EXPECT_TRUE(result.exact_continuation);
    EXPECT_GT(result.visited, 1U);
}

TEST(PreCheckTest, HeuristicKktSearchVerifiesNegativeIntermediateValuesBeforeKkt)
{
    const auto result = pre_check::detail::run_heuristic_kkt_search(symmetric_matrix(3, {-2, 1, -6, 19, -14, -5}));
    ASSERT_TRUE(result.sign);
    EXPECT_EQ(*result.sign, -1);
    EXPECT_FALSE(result.reached_kkt);
    EXPECT_EQ(result.visited, 2U);
}

TEST(PreCheckTest, OneExactFactorizationDecidesDefinitenessAndNullityOneByMode)
{
    pre_check::options selected = pre_check::options::none();
    selected.positive_definiteness = true;
    size_t calls = 0;

    const matrix_integer positive_definite = symmetric_matrix(2, {2, -1, 2});
    EXPECT_TRUE(pre_check::check(positive_definite, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_TRUE(pre_check::check(positive_definite, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_EQ(calls, 0U);

    const matrix_integer positive_semidefinite = symmetric_matrix(2, {1, -1, 1});
    EXPECT_FALSE(pre_check::check(positive_semidefinite, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_TRUE(pre_check::check(positive_semidefinite, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_EQ(calls, 0U);

    // The unique kernel direction has mixed signs, so it cannot contain a nonzero nonnegative vector.
    const matrix_integer singular_strict = symmetric_matrix(2, {1, 1, 1});
    EXPECT_TRUE(pre_check::check(singular_strict, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_EQ(calls, 0U);

    // A singular PSD Z-matrix is copositive but not strictly copositive even when its nullity exceeds one.
    matrix_integer higher_nullity(3, 3);
    higher_nullity(0, 0).set_one();
    EXPECT_FALSE(pre_check::check(higher_nullity, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_EQ(calls, 0U);

    // Failure of both factorization certificates is inconclusive. Horn's copositive matrix plus the identity is strictly
    // copositive and indefinite, while stripping its positive off-diagonal entries leaves the singular cycle Laplacian.
    const matrix_integer indefinite_strict =
        symmetric_matrix(5, {2, -1, 1, 1, -1, 2, -1, 1, 1, 2, -1, 1, 2, -1, 2});
    EXPECT_TRUE(pre_check::check(indefinite_strict, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

TEST(PreCheckTest, ExactNegativePartFactorizationAddsAnSpnCertificate)
{
    pre_check::options selected = pre_check::options::none();
    selected.positive_definiteness = true;

    // The full matrix is indefinite, but replacing its positive off-diagonal entries by zero gives the positive-definite
    // tridiagonal matrix with leading principal determinants 2, 3, 4, and 5.
    const matrix_integer strict_spn = symmetric_matrix(4, {2, -3, 10, 10, 6, -1, 10, 2, -1, 2});
    size_t calls = 0;
    const auto strict_result = pre_check::classify(strict_spn, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{false, false};
    });
    EXPECT_TRUE(strict_result.is_copositive);
    EXPECT_TRUE(strict_result.is_strictly_copositive);
    EXPECT_EQ(calls, 0U);

    // A singular stripped matrix proves ordinary copositivity only. Positive entries can make the original matrix indefinite and
    // strictly copositive, so the strict predicate must remain delegated rather than being reported as false.
    const matrix_integer boundary_spn = symmetric_matrix(4, {1, -1, 10, 10, 2, -1, 10, 2, -1, 1});
    EXPECT_TRUE(pre_check::check(boundary_spn, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return false;
    }));
    EXPECT_TRUE(pre_check::check(boundary_spn, strict, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    EXPECT_EQ(calls, 1U);
}

TEST(PreCheckTest, ExactFullZFactorizationDoesNotFallThroughToMaximalBlocks)
{
    pre_check::options selected = pre_check::options::none();
    selected.positive_definiteness = true;
    selected.z_matrix = true;

    // This connected Z-matrix is indefinite. Two distinct negative off-diagonal values keep it out of the specialized
    // Motzkin--Straus path, so its first exact factorization must reject it without repeating the work as a maximal Z-block.
    const matrix_integer matrix = symmetric_matrix(4, {5, -1, -2, -2, 5, -2, -2, 5, -2, 5});
    diagnostics::detail::reset();
    diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
    size_t calls = 0;
    EXPECT_FALSE(pre_check::check(matrix, copositive, selected, [&](const matrix_integer&) {
        ++calls;
        return true;
    }));
    const diagnostics::snapshot snapshot = diagnostics::detail::load();
    diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
    diagnostics::detail::reset();

    EXPECT_EQ(calls, 0U);
    EXPECT_EQ(snapshot.phase, diagnostics::preprocessing_phase::exact_factorization);
}

TEST(PreCheckTest, CombinedClassificationPreservesTheStrictBoundaryInOneTraversal)
{
    pre_check::options selected = pre_check::options::none();
    selected.small_dimension = true;
    const matrix_integer small_boundary = symmetric_matrix(2, {1, -1, 1});
    size_t calls = 0;
    auto result = pre_check::classify(small_boundary, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{false, false};
    });
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 0U);

    // Frank-Wolfe records the exact zero, and the same matrix's one LDLT factorization proves non-strict copositivity.
    selected = pre_check::options::none();
    selected.frank_wolfe = true;
    selected.positive_definiteness = true;
    const matrix_integer zero_at_centre = constant_off_diagonal(4, 3, -1);
    result = pre_check::classify(zero_at_centre, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{false, false};
    });
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 0U);

    selected = pre_check::options::none();
    selected.nonnegative_off_diagonal = true;
    const matrix_integer zero_diagonal = symmetric_matrix(2, {0, 1, 1});
    result = pre_check::classify(zero_diagonal, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{false, false};
    });
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 0U);

    selected = pre_check::options::none();
    selected.principal_submatrices = true;
    selected.principal_submatrices_up_to = 1;
    const matrix_integer principal_boundary = symmetric_matrix(4, {0, 0, 0, 0, 1, 0, 0, 1, 0, 1});
    result = pre_check::classify(principal_boundary, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{true, true};
    });
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 1U);

    selected = pre_check::options::none();
    selected.positive_definiteness = true;
    const matrix_integer positive_definite = symmetric_matrix(2, {2, -1, 2});
    result = pre_check::classify(positive_definite, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{false, false};
    });
    EXPECT_TRUE(result.is_copositive);
    EXPECT_TRUE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 1U);

    const matrix_integer positive_semidefinite = symmetric_matrix(2, {1, -1, 1});
    result = pre_check::classify(positive_semidefinite, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{true, false};
    });
    EXPECT_TRUE(result.is_copositive);
    EXPECT_FALSE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 1U);

    const matrix_integer singular_strict = symmetric_matrix(2, {1, 1, 1});
    result = pre_check::classify(singular_strict, selected, [&](const matrix_integer&) {
        ++calls;
        return model::copositivity_classification{false, false};
    });
    EXPECT_TRUE(result.is_copositive);
    EXPECT_TRUE(result.is_strictly_copositive);
    EXPECT_EQ(calls, 1U);
}

TEST(PreCheckTest, RejectsInvalidPrincipalSubmatrixCutoffs)
{
    const auto final_algorithm = [](const matrix_integer&) { return true; };
    pre_check::options invalid_cutoff = pre_check::options::none();
    invalid_cutoff.principal_submatrices = true;
    invalid_cutoff.principal_submatrices_up_to = 0;
    EXPECT_THROW(pre_check::check(symmetric_matrix(4, {1, 0, 0, 0, 1, 0, 0, 1, 0, 1}), copositive,
                                  invalid_cutoff, final_algorithm), std::invalid_argument);
    invalid_cutoff.principal_submatrices_up_to = 4;
    EXPECT_THROW(pre_check::check(symmetric_matrix(4, {1, 0, 0, 0, 1, 0, 0, 1, 0, 1}), copositive,
                                  invalid_cutoff, final_algorithm), std::invalid_argument);
}

} // namespace
