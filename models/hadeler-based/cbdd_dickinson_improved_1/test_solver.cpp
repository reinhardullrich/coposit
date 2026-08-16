#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
std::vector<std::pair<uint64_t, uint64_t>>
cbdd_dickinson_improved_1_singular_candidates(
    const matrix_integer &matrix, const std::vector<size_t> &indices);
size_t cbdd_dickinson_improved_1_retained_interval_count(
    size_t dimension,
    const std::vector<std::pair<uint64_t, uint64_t>> &intervals);
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

size_t event_count(std::string_view name, size_t value) {
  return static_cast<size_t>(
      std::count(cbdd_dickinson_improved_1_diagnostics::events.begin(),
                 cbdd_dickinson_improved_1_diagnostics::events.end(),
                 cbdd_dickinson_improved_1_diagnostics::event{name, value}));
}

bool contains_interval(
    const std::vector<std::pair<uint64_t, uint64_t>> &intervals, uint64_t lower,
    uint64_t upper) {
  return std::find(intervals.begin(), intervals.end(),
                   std::pair<uint64_t, uint64_t>{lower, upper}) !=
         intervals.end();
}

TEST(CbddDickinsonImproved1Test, PreservesBasicStrictAndOrdinaryDecisions) {
  const auto ordinary = model::copositivity_mode::copositive;
  EXPECT_TRUE(model::solve(symmetric_matrix(1, {1})));
  EXPECT_FALSE(model::solve(symmetric_matrix(1, {0})));
  EXPECT_TRUE(model::solve(symmetric_matrix(1, {0}), ordinary));
  EXPECT_TRUE(model::solve(symmetric_matrix(2, {1, -1, 1}), ordinary));
  EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 1}), ordinary));
}

TEST(CbddDickinsonImproved1Test, ClassifiesBothPredicatesInOneTraversal) {
  const auto strict = model::classify(symmetric_matrix(2, {1, 0, 1}));
  EXPECT_TRUE(strict.is_copositive);
  EXPECT_TRUE(strict.is_strictly_copositive);

  const auto boundary = model::classify(symmetric_matrix(2, {1, -1, 1}));
  EXPECT_TRUE(boundary.is_copositive);
  EXPECT_FALSE(boundary.is_strictly_copositive);

  const auto negative = model::classify(symmetric_matrix(2, {1, -2, 1}));
  EXPECT_FALSE(negative.is_copositive);
  EXPECT_FALSE(negative.is_strictly_copositive);
}

TEST(CbddDickinsonImproved1Test, KeepsBothNullityOneOrientations) {
  matrix_integer matrix(4, 4);
  matrix(0, 0) = integer(4);
  matrix(0, 1) = matrix(1, 0) = integer(2);
  matrix(1, 1) = integer(1);
  matrix(2, 0) = matrix(0, 2) = integer(1);
  matrix(3, 1) = matrix(1, 3) = integer(1);
  cbdd_dickinson_improved_1_diagnostics::clear();

  const auto intervals =
      model::cbdd_dickinson_improved_1_singular_candidates(matrix, {0, 1});

  EXPECT_TRUE(contains_interval(intervals, 0b0011, 0b0111));
  EXPECT_TRUE(contains_interval(intervals, 0b0011, 0b1011));
  EXPECT_EQ(event_count("affine-inconsistent", 1), 1U);
  EXPECT_EQ(event_count("homogeneous-candidate", 2), 2U);
}

TEST(CbddDickinsonImproved1Test,
     UsesAConsistentSingularAffineParticularSolution) {
  const matrix_integer matrix = symmetric_matrix(3, {1, 1, 0, 1, 0, 0});
  cbdd_dickinson_improved_1_diagnostics::clear();

  const auto intervals =
      model::cbdd_dickinson_improved_1_singular_candidates(matrix, {0, 1});

  EXPECT_FALSE(intervals.empty());
  EXPECT_EQ(event_count("affine-consistent", 1), 1U);
  EXPECT_EQ(event_count("affine-candidate", 1), 1U);
}

TEST(CbddDickinsonImproved1Test,
     RecognizesANonpositiveAffineSolutionAsANegativeWitness) {
  const matrix_integer matrix = symmetric_matrix(2, {-1, -1, -1});
  cbdd_dickinson_improved_1_diagnostics::clear();

  EXPECT_TRUE(
      model::cbdd_dickinson_improved_1_singular_candidates(matrix, {0, 1})
          .empty());
  EXPECT_EQ(event_count("affine-consistent", 1), 1U);
  EXPECT_EQ(event_count("negative-affine-witness", 2), 1U);
}

TEST(CbddDickinsonImproved1Test, EnumeratesTheCompleteStackedPlanarFamily) {
  matrix_integer matrix(5, 5);
  for (size_t row = 0; row < 3; ++row)
    for (size_t column = 0; column < 3; ++column)
      matrix(row, column).set_one();
  const slong outside_rows[2][3] = {{1, 0, 1}, {0, 1, 0}};
  for (size_t outside = 0; outside < 2; ++outside) {
    for (size_t local = 0; local < 3; ++local) {
      matrix(3 + outside, local) = integer(outside_rows[outside][local]);
      matrix(local, 3 + outside) = matrix(3 + outside, local);
    }
  }
  cbdd_dickinson_improved_1_diagnostics::clear();

  const auto intervals =
      model::cbdd_dickinson_improved_1_singular_candidates(matrix, {0, 1, 2});

  EXPECT_TRUE(contains_interval(intervals, 0b00011, 0b01111));
  EXPECT_TRUE(contains_interval(intervals, 0b00110, 0b10111));
  EXPECT_TRUE(contains_interval(intervals, 0b00101, 0b11111));
  EXPECT_GE(event_count("stacked-line", 2), 3U);
  EXPECT_EQ(event_count("affine-consistent", 2), 1U);
}

TEST(CbddDickinsonImproved1Test,
     RejectsAnIntervalCoveredOnlyByTheExistingUnion) {
  const std::vector<std::pair<uint64_t, uint64_t>> intervals{
      {0b001, 0b011},
      {0b101, 0b111},
      {0b001, 0b111},
  };
  EXPECT_EQ(
      model::cbdd_dickinson_improved_1_retained_interval_count(3, intervals),
      2U);
  EXPECT_EQ(model::cbdd_dickinson_improved_1_retained_interval_count(
                3, {{0b001, 0b111}, {0b001, 0b011}, {0b101, 0b111}}),
            1U);
}

TEST(CbddDickinsonImproved1Test,
     LeavesHigherNullityOnTheOriginalSingleVectorFallback) {
  matrix_integer zero(3, 3);
  cbdd_dickinson_improved_1_diagnostics::clear();

  const auto intervals =
      model::cbdd_dickinson_improved_1_singular_candidates(zero, {0, 1, 2});

  EXPECT_FALSE(intervals.empty());
  EXPECT_EQ(event_count("higher-nullity-fallback", 3), 1U);
}

TEST(CbddDickinsonImproved1Test,
     PublishesOnlyMarginallyRetainedCertificateIntervals) {
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

TEST(CbddDickinsonImproved1Test, KeepsPackedSupportsBeyondOneWord) {
  matrix_integer matrix;
  matrix.set_identity(65);
  matrix(63, 64) = integer(-2);
  matrix(64, 63) = integer(-2);
  EXPECT_FALSE(model::solve(matrix));
}

} // namespace
