#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>

#include "source_diagnostics.hpp"

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
std::vector<uint64_t> f1_generated_masks(size_t dimension,
                                         uint64_t forbidden_trigger);
bool f1_check_support_for_testing(const matrix_integer &matrix,
                                  const std::vector<size_t> &indices);
} // namespace coposit::model

namespace {

matrix_integer symmetric_matrix(size_t dimension,
                                std::initializer_list<slong> upper_triangle) {
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

TEST(F1Test, PreservesStrictAndCopositiveDecisions) {
  EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
  EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
  EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}),
                           model::copositivity_mode::copositive));
  EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
  EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}),
                           model::copositivity_mode::copositive));
  EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
  EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}),
                            model::copositivity_mode::copositive));
}

TEST(F1Test, ClassifiesBothPredicatesInOneTraversal) {
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

TEST(F1Test, MatchesTheIndependentExactThreeByThreeCriterion) {
  for (slong a00 = -1; a00 <= 1; ++a00) {
    for (slong a01 = -1; a01 <= 1; ++a01) {
      for (slong a02 = -1; a02 <= 1; ++a02) {
        for (slong a11 = -1; a11 <= 1; ++a11) {
          for (slong a12 = -1; a12 <= 1; ++a12) {
            for (slong a22 = -1; a22 <= 1; ++a22) {
              const matrix_integer matrix =
                  symmetric_matrix(3, {a00, a01, a02, a11, a12, a22});
              const auto expected = small_copositivity::classify(matrix);
              const auto actual = model::classify(matrix);

              SCOPED_TRACE(::testing::Message()
                           << "A=[" << a00 << ',' << a01 << ',' << a02 << ';'
                           << a11 << ',' << a12 << ';' << a22 << ']');
              EXPECT_EQ(actual.is_copositive, expected.is_copositive);
              EXPECT_EQ(actual.is_strictly_copositive,
                        expected.is_strictly_copositive);
            }
          }
        }
      }
    }
  }
}

TEST(F1Test, KeepsTriggerButSkipsEveryLargerSuperset) {
  const std::vector<uint64_t> generated = model::f1_generated_masks(4, 0b0101);
  EXPECT_EQ(generated.size(), 12U);
  EXPECT_NE(std::find(generated.begin(), generated.end(), 0b0101),
            generated.end());
  EXPECT_EQ(std::find(generated.begin(), generated.end(), 0b0111),
            generated.end());
  EXPECT_EQ(std::find(generated.begin(), generated.end(), 0b1101),
            generated.end());
  EXPECT_EQ(std::find(generated.begin(), generated.end(), 0b1111),
            generated.end());
}

TEST(F1Test, ProcessesEachSupportOnceWithoutExactSupportClauses) {
  std::vector<uint64_t> generated = model::f1_generated_masks(4, 0);
  EXPECT_EQ(generated.size(), 15U);
  std::sort(generated.begin(), generated.end());
  EXPECT_EQ(std::adjacent_find(generated.begin(), generated.end()),
            generated.end());
}

TEST(F1Test, StoresOnlyDickinsonCertificatesThatReachTheCeiling) {
  f1_diagnostics::clear();
  EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));

  EXPECT_EQ(std::count(f1_diagnostics::events.begin(),
                       f1_diagnostics::events.end(),
                       f1_diagnostics::event{"discard-certificate", 1}),
            2);
  EXPECT_EQ(std::count(f1_diagnostics::events.begin(),
                       f1_diagnostics::events.end(),
                       f1_diagnostics::event{"ceiling-certificate", 2}),
            1);
}

TEST(F1Test, InstallsExactPairCurvatureCertificatesBeforeTraversal) {
  f1_diagnostics::clear();
  EXPECT_TRUE(model::solve(symmetric_matrix(3, {4, 5, -1, 4, -1, 4})));

  EXPECT_NE(std::find(f1_diagnostics::events.begin(),
                      f1_diagnostics::events.end(),
                      f1_diagnostics::event{"pair-curvature-certificate", 2}),
            f1_diagnostics::events.end());
}

TEST(F1Test, ChecksCurvatureAtStagedDickinsonEndpointsAndStillOptimizes) {
  f1_diagnostics::clear();
  const matrix_integer matrix =
      symmetric_matrix(4, {2, 3, 1, 0, 10, 0, 1, 0, 0, 1});

  EXPECT_TRUE(model::f1_check_support_for_testing(matrix, {0, 1}));
  EXPECT_NE(
      std::find(f1_diagnostics::events.begin(), f1_diagnostics::events.end(),
                f1_diagnostics::event{"endpoint-traditional-curvature", 3}),
      f1_diagnostics::events.end());
  EXPECT_NE(std::find(f1_diagnostics::events.begin(),
                      f1_diagnostics::events.end(),
                      f1_diagnostics::event{"halfspace-improvement", 2}),
            f1_diagnostics::events.end());
}

TEST(F1Test, FloatingGoodEndpointOnlySkipsTheOptionalLookahead) {
  f1_diagnostics::clear();
  const matrix_integer matrix =
      symmetric_matrix(4, {1, 0, 1, -1, 1, 0, -1, 10, 0, 1});

  EXPECT_TRUE(model::f1_check_support_for_testing(matrix, {0, 1}));
  EXPECT_NE(
      std::find(f1_diagnostics::events.begin(), f1_diagnostics::events.end(),
                f1_diagnostics::event{"endpoint-curvature-screened-good", 3}),
      f1_diagnostics::events.end());
  EXPECT_EQ(
      std::find(f1_diagnostics::events.begin(), f1_diagnostics::events.end(),
                f1_diagnostics::event{"endpoint-traditional-curvature", 3}),
      f1_diagnostics::events.end());
}

TEST(F1Test, PublishesDiagnosticsAndCeilingCertificateDiagnostics) {
  matrix_integer identity;
  identity.set_identity(2);

  diagnostics::detail::reset();
  diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
  EXPECT_TRUE(model::solve(identity));
  const diagnostics::snapshot snapshot = diagnostics::detail::load();
  diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
  diagnostics::detail::reset();

  EXPECT_EQ(snapshot.kind, diagnostics::metric::support);
  EXPECT_EQ(snapshot.nodes, 3U);
  EXPECT_EQ(snapshot.resolved, 1U);
  EXPECT_EQ(snapshot.secondary, 2U);
  EXPECT_EQ(snapshot.splits, 2U);
  EXPECT_EQ(snapshot.open, 0U);
  EXPECT_EQ(snapshot.certificate_cardinality_free_index_counts,
            (std::map<std::pair<size_t, size_t>, uint64_t>{{{1, 1}, 2}}));
}

TEST(F1Test, RejectsANonCopositiveMatrix) {
  EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 2}),
                            model::copositivity_mode::copositive));
}

TEST(F1Test, KeepsPackedSupportsBeyondOneWord) {
  matrix_integer matrix;
  matrix.set_identity(65);
  matrix(63, 64) = integer(-2);
  matrix(64, 63) = integer(-2);
  EXPECT_FALSE(model::solve(matrix));
}

} // namespace
