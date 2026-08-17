#include "source_diagnostics.hpp"
#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <map>

using namespace coposit;

namespace coposit::model {
bool clingo_halfspace_prefers_negative_singular_orientation_for_testing(
    size_t positive_products, size_t negative_products) noexcept;
size_t clingo_halfspace_optimized_certificate_count_for_testing() noexcept;
}

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

TEST(ClingoHalfspaceDickinsonTest, PreservesStrictDickinsonDecisions) {
  EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
  EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
  EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));
  EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -1, 1})));
  EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1})));
}

TEST(ClingoHalfspaceDickinsonTest,
     DistinguishesNonStrictFromStrictCopositivity) {
  const auto copositive = model::copositivity_mode::copositive;
  EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), copositive));
  EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), copositive));
  EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), copositive));
}

TEST(ClingoHalfspaceDickinsonTest, ClassifiesBothPredicatesInOneTraversal) {
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

TEST(ClingoHalfspaceDickinsonTest, SelectsAnExactImprovingHalfspaceDirection) {
  EXPECT_TRUE(
      model::solve(symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14})));
  EXPECT_GT(model::clingo_halfspace_optimized_certificate_count_for_testing(),
            0U);
}

TEST(ClingoHalfspaceDickinsonTest,
     ChoosesTheSingularOrientationWithTheLargerUpperSet) {
  EXPECT_TRUE(
      model::clingo_halfspace_prefers_negative_singular_orientation_for_testing(
          1, 2));
  EXPECT_FALSE(
      model::clingo_halfspace_prefers_negative_singular_orientation_for_testing(
          2, 1));
  EXPECT_FALSE(
      model::clingo_halfspace_prefers_negative_singular_orientation_for_testing(
          2, 2));
}

TEST(ClingoHalfspaceDickinsonTest,
     KeepsSingletonCertificatesAcrossLaterCardinalities) {
  matrix_integer identity;
  identity.set_identity(4);
  clingo_halfspace_dickinson_diagnostics::clear();
  EXPECT_TRUE(model::solve(identity));

  ASSERT_EQ(clingo_halfspace_dickinson_diagnostics::events.size(), 4U);
  for (const auto &event : clingo_halfspace_dickinson_diagnostics::events)
    EXPECT_EQ(event,
              (clingo_halfspace_dickinson_diagnostics::event{"process", 1}));
}

TEST(ClingoHalfspaceDickinsonTest,
     KeepsPartialSingletonIntervalsAcrossCardinalityCalls) {
  const matrix_integer matrix = symmetric_matrix(3, {1, 1, -1, 1, -1, 1});
  clingo_halfspace_dickinson_diagnostics::clear();
  const auto classification = model::classify(matrix);

  EXPECT_TRUE(classification.is_copositive);
  EXPECT_FALSE(classification.is_strictly_copositive);
  std::map<size_t, size_t> processed;
  for (const auto &event : clingo_halfspace_dickinson_diagnostics::events)
    if (event.name == "process")
      ++processed[event.cardinality];
  EXPECT_EQ(processed, (std::map<size_t, size_t>{{1, 3}, {2, 2}}));
  EXPECT_NE(std::find(clingo_halfspace_dickinson_diagnostics::events.begin(),
                      clingo_halfspace_dickinson_diagnostics::events.end(),
                      clingo_halfspace_dickinson_diagnostics::event{
                          "expiry_guard", 2}),
            clingo_halfspace_dickinson_diagnostics::events.end());
}

TEST(ClingoHalfspaceDickinsonTest,
     ExhaustsEverySupportWhenAllDickinsonIntervalsAreSingletons) {
  constexpr size_t dimension = 8;
  matrix_integer matrix(dimension, dimension);
  for (size_t row = 0; row < dimension; ++row) {
    for (size_t column = 0; column < dimension; ++column)
      matrix(row, column) =
          integer(row == column ? static_cast<slong>(dimension) : -1);
  }

  clingo_halfspace_dickinson_diagnostics::clear();
  EXPECT_TRUE(model::solve(matrix));

  std::map<size_t, size_t> processed;
  for (const auto &event : clingo_halfspace_dickinson_diagnostics::events)
    if (event.name == "process")
      ++processed[event.cardinality];
  EXPECT_EQ(processed, (std::map<size_t, size_t>{{1, 8},
                                                 {2, 28},
                                                 {3, 56},
                                                 {4, 70},
                                                 {5, 56},
                                                 {6, 28},
                                                 {7, 8},
                                                 {8, 1}}));
  EXPECT_NE(std::find_if(clingo_halfspace_dickinson_diagnostics::events.begin(),
                         clingo_halfspace_dickinson_diagnostics::events.end(),
                         [](const auto &event) {
                           return event.name == "current_layer_only";
                         }),
            clingo_halfspace_dickinson_diagnostics::events.end());
}

TEST(ClingoHalfspaceDickinsonTest,
     PublishesCertificateFreeIndexAndUpperSizeDistributionOnlyWithDiagnostics) {
  matrix_integer identity;
  identity.set_identity(2);

  diagnostics::detail::reset();
  diagnostics::detail::state.enabled.store(true, std::memory_order_relaxed);
  EXPECT_TRUE(model::solve(identity));
  const diagnostics::snapshot snapshot = diagnostics::detail::load();
  diagnostics::detail::state.enabled.store(false, std::memory_order_relaxed);
  diagnostics::detail::reset();

  EXPECT_EQ(
      snapshot.certificate_cardinality_free_index_upper_size_counts,
      (std::map<std::tuple<size_t, size_t, size_t>, uint64_t>{{{1, 1, 2}, 2}}));
}

TEST(ClingoHalfspaceDickinsonTest, AcceptsABoundaryMatrixInNonStrictMode) {
  EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -2, 4}),
                           model::copositivity_mode::copositive));
}

TEST(ClingoHalfspaceDickinsonTest, HandlesSupportAtomsBeyondOneWord) {
  matrix_integer matrix;
  matrix.set_identity(65);
  matrix(63, 64) = integer(-2);
  matrix(64, 63) = integer(-2);

  EXPECT_FALSE(model::solve(matrix));
}

} // namespace
