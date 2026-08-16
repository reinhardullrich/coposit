#include <coposit/model.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
bool affine_companion_search_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool affine_companion_strict_search_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices);
bool affine_companion_affine_search_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices);
} // namespace coposit::model

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

matrix_integer zero_root_with_projected_rows(std::initializer_list<std::pair<slong, slong>> rows)
{
    matrix_integer matrix(2 + rows.size(), 2 + rows.size());
    size_t outside = 2;
    for (const auto& [first, second] : rows) {
        matrix(outside, 0) = integer(first);
        matrix(0, outside) = matrix(outside, 0);
        matrix(outside, 1) = integer(second);
        matrix(1, outside) = matrix(outside, 1);
        ++outside;
    }
    return matrix;
}

size_t event_count(std::string_view name, size_t cardinality)
{
    return static_cast<size_t>(std::count(affine_companion_dickinson_diagnostics::events.begin(),
        affine_companion_dickinson_diagnostics::events.end(), affine_companion_dickinson_diagnostics::event { name, cardinality }));
}

TEST(AffineCompanionDickinsonTest, UsesAPersistentKernelOfDimensionAtLeastTwoDirectly)
{
    matrix_integer zero(4, 4);
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_search_for_test(zero, { 0, 1 }));
    EXPECT_EQ(event_count("persistent-kernel", 2), 1U);
    EXPECT_EQ(event_count("persistent-certificate", 1), 1U);
    EXPECT_EQ(event_count("ceiling-certificate", 1), 1U);
}

TEST(AffineCompanionDickinsonTest, FindsExtremeRaysWhenThePersistentKernelIsZero)
{
    matrix_integer matrix(6, 6);
    for (size_t coordinate = 0; coordinate < 3; ++coordinate) {
        matrix(3 + coordinate, coordinate) = integer(1);
        matrix(coordinate, 3 + coordinate) = integer(1);
    }
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_search_for_test(matrix, { 0, 1, 2 }));
    EXPECT_EQ(event_count("persistent-kernel", 0), 1U);
    EXPECT_EQ(event_count("active-ray-certificate", 1), 3U);
}

TEST(AffineCompanionDickinsonTest, EnumeratesAntipodalConstraintsAsOneActiveHyperplane)
{
    matrix_integer matrix(8, 8);
    const slong rows[5][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 } };
    for (size_t outside = 0; outside < 5; ++outside) {
        for (size_t coordinate = 0; coordinate < 3; ++coordinate) {
            matrix(3 + outside, coordinate) = integer(rows[outside][coordinate]);
            matrix(coordinate, 3 + outside) = matrix(3 + outside, coordinate);
        }
    }
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_search_for_test(matrix, { 0, 1, 2 }));
    EXPECT_EQ(event_count("active-hyperplanes", 3), 1U);
    EXPECT_EQ(event_count("active-set", 3), 3U);
    EXPECT_EQ(event_count("active-ray-certificate", 1), 1U);
}

TEST(AffineCompanionDickinsonTest, FallsBackWhenTheRootConeHasNoNonzeroRay)
{
    matrix_integer matrix = zero_root_with_projected_rows({ { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } });
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_search_for_test(matrix, { 0, 1 }));
    EXPECT_EQ(event_count("persistent-kernel", 0), 1U);
    EXPECT_EQ(event_count("active-ray-certificate", 1), 0U);
    EXPECT_EQ(event_count("ceiling-certificate", 1), 0U);
}

TEST(AffineCompanionDickinsonTest, NormalizesProjectedRowsBeforeThePlanarSearch)
{
    matrix_integer matrix = zero_root_with_projected_rows({ { 1, 0 }, { 2, 0 }, { 0, 1 } });
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_search_for_test(matrix, { 0, 1 }));
    EXPECT_EQ(event_count("active-ray-certificate", 1), 2U);
    EXPECT_EQ(event_count("projected-constraints", 2), 1U);
}

TEST(AffineCompanionDickinsonTest, FindsTheSingleRayOfAPlanarHalfPlane)
{
    matrix_integer matrix = zero_root_with_projected_rows({ { 1, 0 }, { -1, 0 }, { 0, 1 } });
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_search_for_test(matrix, { 0, 1 }));
    EXPECT_EQ(event_count("active-ray-certificate", 1), 1U);
    EXPECT_EQ(event_count("duplicate-ray", 2), 1U);
}

TEST(AffineCompanionDickinsonTest, PreservesOneSidedZeroChecksForNonBoundaryPlanarRows)
{
    matrix_integer matrix = zero_root_with_projected_rows({ { 1, -2 }, { 1, -1 }, { 1, 2 } });
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_FALSE(model::affine_companion_strict_search_for_test(matrix, { 0, 1 }));
}

TEST(AffineCompanionDickinsonTest, RecoversAMinimumSupportAffineCertificateAtTheSingularFace)
{
    const matrix_integer matrix = symmetric_matrix(3, { 1, 1, 0, 1, 2, 0 });
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_affine_search_for_test(matrix, { 0, 1 }));
    EXPECT_EQ(event_count("affine-consistent", 1), 1U);
    EXPECT_EQ(event_count("affine-certificate", 1), 1U);
    EXPECT_EQ(event_count("projected-build", 1), 1U);
}

TEST(AffineCompanionDickinsonTest, RejectsAnInconsistentAffineSystemWithoutSolvingIt)
{
    const matrix_integer matrix = symmetric_matrix(2, { 1, 0, 0 });
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_affine_search_for_test(matrix, { 0, 1 }));
    EXPECT_EQ(event_count("affine-inconsistent", 1), 1U);
    EXPECT_EQ(event_count("affine-certificate", 1), 0U);
    EXPECT_EQ(event_count("projected-build", 1), 0U);
}

TEST(AffineCompanionDickinsonTest, LeavesAnEmptyAffineCeilingIntersectionToTheFallback)
{
    matrix_integer matrix(6, 6);
    matrix(0, 0).set_one();
    matrix(0, 1).set_one();
    matrix(1, 0).set_one();
    matrix(1, 1).set_one();
    const slong rows[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (size_t outside = 0; outside < 4; ++outside) {
        matrix(2 + outside, 0) = integer(rows[outside][0]);
        matrix(0, 2 + outside) = matrix(2 + outside, 0);
        matrix(2 + outside, 1) = integer(rows[outside][1]);
        matrix(1, 2 + outside) = matrix(2 + outside, 1);
    }
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_affine_search_for_test(matrix, { 0, 1 }));
    EXPECT_EQ(event_count("affine-consistent", 1), 1U);
    EXPECT_EQ(event_count("affine-certificate", 1), 0U);
}

TEST(AffineCompanionDickinsonTest, TestsOneParticularSolutionWhenNullityIsLargerThanOne)
{
    const matrix_integer matrix = symmetric_matrix(3, { 1, 1, 1, 1, 1, 1 });
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_TRUE(model::affine_companion_affine_search_for_test(matrix, { 0, 1, 2 }));
    EXPECT_EQ(event_count("affine-consistent", 2), 1U);
    EXPECT_EQ(event_count("affine-certificate", 1), 1U);
    EXPECT_EQ(event_count("projected-build", 2), 0U);
}

TEST(AffineCompanionDickinsonTest, RejectsANonpositiveAffineSolutionAsANegativeWitness)
{
    const matrix_integer matrix = symmetric_matrix(2, { -1, -1, -1 });
    affine_companion_dickinson_diagnostics::clear();

    EXPECT_FALSE(model::affine_companion_affine_search_for_test(matrix, { 0, 1 }));
    EXPECT_EQ(event_count("affine-consistent", 1), 1U);
}

TEST(AffineCompanionDickinsonTest, ClassifiesBothPredicatesInOneTraversal)
{
    const auto strict = model::classify(symmetric_matrix(2, { 1, 0, 1 }));
    EXPECT_TRUE(strict.is_copositive);
    EXPECT_TRUE(strict.is_strictly_copositive);

    const auto boundary = model::classify(symmetric_matrix(2, { 1, -1, 1 }));
    EXPECT_TRUE(boundary.is_copositive);
    EXPECT_FALSE(boundary.is_strictly_copositive);

    const auto negative = model::classify(symmetric_matrix(2, { 1, -2, 1 }));
    EXPECT_FALSE(negative.is_copositive);
    EXPECT_FALSE(negative.is_strictly_copositive);
}

} // namespace
