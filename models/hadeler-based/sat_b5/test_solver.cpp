#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
size_t sat_b5_pair_upward_count_for_testing() noexcept;
size_t sat_b5_support_upward_count_for_testing() noexcept;
size_t sat_b5_optimized_certificate_count_for_testing() noexcept;
bool sat_b5_schur_reduction_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& reduced);
bool sat_b5_check_support_for_testing(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool sat_b5_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper);
size_t sat_b5_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::vector<size_t>>& upward,
    const std::vector<std::vector<size_t>>& exact);
size_t sat_b5_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals);
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

TEST(SatB5Test, IterativelyReducesPositiveDefiniteLowSupports)
{
    sat_b5_diagnostics::clear();
    matrix_integer identity;
    identity.set_identity(8);
    const auto classification = model::classify(identity);

    EXPECT_TRUE(classification.is_copositive);
    EXPECT_TRUE(classification.is_strictly_copositive);
    EXPECT_EQ(std::count(sat_b5_diagnostics::events.begin(), sat_b5_diagnostics::events.end(),
                         sat_b5_diagnostics::event{"schur_reduction", 1}),
              7);
}

TEST(SatB5Test, HasNoHighFrontierOrDownwardPruning)
{
    sat_b5_diagnostics::clear();
    (void)model::classify(symmetric_matrix(3, {2, 1, -4, 2, 1, 2}));

    EXPECT_EQ(std::count_if(sat_b5_diagnostics::events.begin(), sat_b5_diagnostics::events.end(), [](const auto& event) {
                  return event.name == "process_high" || event.name == "downward" || event.name == "high_float_reject";
              }),
              0);
}

TEST(SatB5Test, BuildsTheExactJohnsonReamsSchurReduction)
{
    const matrix_integer matrix = symmetric_matrix(3, {2, -2, -4, 3, 5, 9});
    matrix_integer reduced;

    ASSERT_TRUE(model::sat_b5_schur_reduction_for_testing(matrix, {0}, reduced));
    ASSERT_EQ(reduced.rows(), 2U);
    EXPECT_EQ(reduced(0, 0).sign(), 1);
    EXPECT_TRUE(reduced(0, 0).is_one());
    EXPECT_TRUE(reduced(0, 1).is_one());
    EXPECT_TRUE(reduced(1, 0).is_one());
    EXPECT_TRUE(reduced(1, 1).is_one());

    const auto original_classification = model::classify(matrix);
    const auto reduced_classification = model::classify(reduced);
    EXPECT_EQ(original_classification.is_copositive, reduced_classification.is_copositive);
    EXPECT_EQ(original_classification.is_strictly_copositive, reduced_classification.is_strictly_copositive);
}

TEST(SatB5Test, ReducesANontrivialPositiveDefiniteBlockAndPreservesBothPredicates)
{
    const auto check = [](const matrix_integer& matrix, slong expected_diagonal, slong expected_off_diagonal,
                          bool expected_copositive, bool expected_strict) {
        matrix_integer reduced;
        const integer diagonal(expected_diagonal);
        const integer off_diagonal(expected_off_diagonal);
        ASSERT_TRUE(model::sat_b5_schur_reduction_for_testing(matrix, {0, 1}, reduced));
        ASSERT_EQ(reduced.rows(), 2U);
        EXPECT_EQ(reduced(0, 0).compare(diagonal), 0);
        EXPECT_EQ(reduced(0, 1).compare(off_diagonal), 0);
        EXPECT_EQ(reduced(1, 0).compare(off_diagonal), 0);
        EXPECT_EQ(reduced(1, 1).compare(diagonal), 0);

        const auto original_classification = model::classify(matrix);
        const auto reduced_classification = model::classify(reduced);
        EXPECT_EQ(original_classification.is_copositive, expected_copositive);
        EXPECT_EQ(original_classification.is_strictly_copositive, expected_strict);
        EXPECT_EQ(original_classification.is_copositive, reduced_classification.is_copositive);
        EXPECT_EQ(original_classification.is_strictly_copositive, reduced_classification.is_strictly_copositive);
    };

    // B=[[2,1],[1,2]], W=[[1,2],[3,1]], and S is respectively positive definite, boundary, or non-copositive.
    check(symmetric_matrix(4, {2, 1, -5, -5, 2, -7, -4, 27, 17, 15}), 1, 0, true, true);
    check(symmetric_matrix(4, {2, 1, -5, -5, 2, -7, -4, 27, 16, 15}), 1, -1, true, false);
    check(symmetric_matrix(4, {2, 1, -5, -5, 2, -7, -4, 27, 15, 15}), 1, -2, false, false);
}

TEST(SatB5Test, RejectsASchurReductionWhenTheEliminatingMapLeavesTheOrthant)
{
    sat_b5_diagnostics::clear();
    matrix_integer reduced;
    EXPECT_FALSE(model::sat_b5_schur_reduction_for_testing(symmetric_matrix(2, {2, 2, 3}), {0}, reduced));
    EXPECT_NE(std::find(sat_b5_diagnostics::events.begin(), sat_b5_diagnostics::events.end(),
                        sat_b5_diagnostics::event{"schur_prefilter_reject", 1}),
              sat_b5_diagnostics::events.end());
}

TEST(SatB5Test, BuildsAnExactDickinsonIntervalOnTheLowPath)
{
    const matrix_integer matrix = symmetric_matrix(3, {1, 1, 0, 1, 0, 1});
    support lower(3);
    support upper(3);

    EXPECT_TRUE(model::sat_b5_certificate_for_testing(matrix, {0}, lower, upper));
    EXPECT_TRUE(lower.contains(0));
    EXPECT_FALSE(lower.contains(1));
    EXPECT_FALSE(lower.contains(2));
    EXPECT_TRUE(upper.contains(0));
    EXPECT_TRUE(upper.contains(1));
    EXPECT_TRUE(upper.contains(2));
}

TEST(SatB5Test, RetainsTheExactHalfspaceRayOptimization)
{
    const matrix_integer matrix = symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14});
    bool improved = false;
    for (uint64_t mask = 1; mask < 16 && !improved; ++mask) {
        std::vector<size_t> indices;
        for (size_t index = 0; index < 4; ++index)
            if ((mask & (uint64_t{1} << index)) != 0) indices.push_back(index);
        (void)model::sat_b5_check_support_for_testing(matrix, indices);
        improved = model::sat_b5_optimized_certificate_count_for_testing() > 0;
    }
    EXPECT_TRUE(improved);
}

TEST(SatB5Test, StoresDickinsonIntervalsInTheSharedSatState)
{
    const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011}, {0b100, 0b110}};
    EXPECT_EQ(model::sat_b5_uncovered_count(3, 1, intervals), 1U);
    EXPECT_EQ(model::sat_b5_uncovered_count(3, 2, intervals), 1U);
}

TEST(SatB5Test, EncodesUpwardAndExactSupportBlocks)
{
    const std::vector<std::vector<size_t>> none;
    const std::vector<std::vector<size_t>> support_01{{0, 1}};

    EXPECT_EQ(model::sat_b5_uncovered_count(3, 1, support_01, none), 3U);
    EXPECT_EQ(model::sat_b5_uncovered_count(3, 2, support_01, none), 2U);
    EXPECT_EQ(model::sat_b5_uncovered_count(3, 3, support_01, none), 0U);

    EXPECT_EQ(model::sat_b5_uncovered_count(3, 1, none, support_01), 3U);
    EXPECT_EQ(model::sat_b5_uncovered_count(3, 2, none, support_01), 2U);
    EXPECT_EQ(model::sat_b5_uncovered_count(3, 3, none, support_01), 1U);
}

TEST(SatB5Test, PreservesExactCombinedClassifications)
{
    const auto strict = model::classify(symmetric_matrix(2, {2, -1, 2}));
    EXPECT_TRUE(strict.is_copositive);
    EXPECT_TRUE(strict.is_strictly_copositive);

    const auto boundary = model::classify(symmetric_matrix(2, {1, -1, 1}));
    EXPECT_TRUE(boundary.is_copositive);
    EXPECT_FALSE(boundary.is_strictly_copositive);

    const auto negative = model::classify(symmetric_matrix(2, {1, -2, 1}));
    EXPECT_FALSE(negative.is_copositive);
    EXPECT_FALSE(negative.is_strictly_copositive);
}

TEST(SatB5Test, ExhaustivelyClassifiesSmallTwoByTwoIntegerMatrices)
{
    for (slong diagonal_0 = -2; diagonal_0 <= 2; ++diagonal_0) {
        for (slong off_diagonal = -2; off_diagonal <= 2; ++off_diagonal) {
            for (slong diagonal_1 = -2; diagonal_1 <= 2; ++diagonal_1) {
                const auto classification = model::classify(
                    symmetric_matrix(2, {diagonal_0, off_diagonal, diagonal_1}));
                const slong determinant = diagonal_0 * diagonal_1 - off_diagonal * off_diagonal;
                const bool expected_copositive = diagonal_0 >= 0 && diagonal_1 >= 0
                    && (off_diagonal >= 0 || determinant >= 0);
                const bool expected_strict = diagonal_0 > 0 && diagonal_1 > 0
                    && (off_diagonal >= 0 || determinant > 0);

                SCOPED_TRACE(::testing::Message()
                             << "A=[" << diagonal_0 << ',' << off_diagonal << ';' << diagonal_1 << ']');
                EXPECT_EQ(classification.is_copositive, expected_copositive);
                EXPECT_EQ(classification.is_strictly_copositive, expected_strict);
            }
        }
    }
}

TEST(SatB5Test, MatchesTheIndependentExactThreeByThreeCriterion)
{
    for (slong a00 = -1; a00 <= 1; ++a00) {
        for (slong a01 = -1; a01 <= 1; ++a01) {
            for (slong a02 = -1; a02 <= 1; ++a02) {
                for (slong a11 = -1; a11 <= 1; ++a11) {
                    for (slong a12 = -1; a12 <= 1; ++a12) {
                        for (slong a22 = -1; a22 <= 1; ++a22) {
                            const matrix_integer matrix = symmetric_matrix(3, {a00, a01, a02, a11, a12, a22});
                            const auto expected = small_copositivity::classify(matrix);
                            const auto actual = model::classify(matrix);

                            SCOPED_TRACE(::testing::Message()
                                         << "A=[" << a00 << ',' << a01 << ',' << a02 << ';' << a11 << ',' << a12
                                         << ';' << a22 << ']');
                            EXPECT_EQ(actual.is_copositive, expected.is_copositive);
                            EXPECT_EQ(actual.is_strictly_copositive, expected.is_strictly_copositive);
                        }
                    }
                }
            }
        }
    }
}

TEST(SatB5Test, AppliesPairCurvaturePrepass)
{
    const auto pair_excluded = model::classify(symmetric_matrix(2, {1, 2, 1}));
    EXPECT_TRUE(pair_excluded.is_copositive);
    EXPECT_TRUE(pair_excluded.is_strictly_copositive);
    EXPECT_EQ(model::sat_b5_pair_upward_count_for_testing(), 1U);
}

} // namespace
