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
std::vector<uint64_t> g1_generated_masks(size_t dimension,
                                         uint64_t forbidden_trigger);
std::vector<uint64_t> g1_compact_masks(size_t dimension,
                                       size_t completed_cardinality,
                                       const std::vector<uint64_t> &roots);
std::pair<bool, size_t> g1_interval_contains(size_t dimension, uint64_t lower,
                                             uint64_t upper, uint64_t candidate,
                                             size_t cardinality);
bool g1_overlapping_intervals_check_curvature(
    size_t dimension, uint64_t first_lower, uint64_t first_upper,
    uint64_t second_lower, uint64_t second_upper, uint64_t candidate);
bool g1_check_support_for_testing(const matrix_integer &matrix,
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

TEST(G1Test, PreservesStrictAndCopositiveDecisions) {
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

TEST(G1Test, ClassifiesBothPredicatesInOneTraversal) {
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

TEST(G1Test, MatchesTheIndependentExactThreeByThreeCriterion) {
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

TEST(G1Test, KeepsTriggerButSkipsEveryLargerSuperset) {
  const std::vector<uint64_t> generated = model::g1_generated_masks(4, 0b0101);
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

TEST(G1Test, ProcessesEachSupportOnceWithoutExactSupportClauses) {
  std::vector<uint64_t> generated = model::g1_generated_masks(4, 0);
  EXPECT_EQ(generated.size(), 15U);
  std::sort(generated.begin(), generated.end());
  EXPECT_EQ(std::adjacent_find(generated.begin(), generated.end()),
            generated.end());
}

TEST(G1Test, CompactsAcrossMultipleRetiredLevelsWithoutChangingFutureCoverage) {
  const std::vector<uint64_t> roots{0b000111, 0b001011, 0b010011, 0b100011,
                                    0b001101, 0b010101, 0b100101, 0b011001,
                                    0b101001, 0b110001};
  const std::vector<uint64_t> compacted = model::g1_compact_masks(6, 3, roots);
  EXPECT_EQ(compacted, (std::vector<uint64_t>{0b000001}));

  const auto covered = [](uint64_t candidate,
                          const std::vector<uint64_t> &family) {
    return std::any_of(family.begin(), family.end(), [&](uint64_t root) {
      return (root & candidate) == root;
    });
  };
  for (uint64_t candidate = 0; candidate < 64; ++candidate) {
    size_t cardinality = 0;
    for (size_t bit = 0; bit < 6; ++bit)
      cardinality += (candidate & (uint64_t{1} << bit)) != 0;
    if (cardinality >= 4)
      EXPECT_EQ(covered(candidate, roots), covered(candidate, compacted));
  }
  EXPECT_FALSE(covered(0b000001, roots));
  EXPECT_TRUE(covered(0b000001, compacted));
}

TEST(G1Test, ExhaustivelyPreservesUnfinishedCoverageForThreeIndices) {
  const auto covered = [](uint64_t candidate,
                          const std::vector<uint64_t> &family) {
    return std::any_of(family.begin(), family.end(), [&](uint64_t root) {
      return (root & candidate) == root;
    });
  };
  const std::vector<uint64_t> possible_roots{1, 2, 3, 4, 5, 6, 7};
  for (uint64_t family_mask = 1; family_mask < 128; ++family_mask) {
    std::vector<uint64_t> roots;
    for (size_t root = 0; root < possible_roots.size(); ++root)
      if ((family_mask & (uint64_t{1} << root)) != 0)
        roots.push_back(possible_roots[root]);

    for (size_t completed_cardinality = 1; completed_cardinality < 3;
         ++completed_cardinality) {
      const std::vector<uint64_t> compacted =
          model::g1_compact_masks(3, completed_cardinality, roots);
      for (uint64_t candidate = 1; candidate < 8; ++candidate) {
        const size_t cardinality = static_cast<size_t>(
            __builtin_popcountll(static_cast<unsigned long long>(candidate)));
        if (cardinality > completed_cardinality)
          EXPECT_EQ(covered(candidate, roots), covered(candidate, compacted));
      }
    }
  }
}

TEST(G1Test, CheaplyCompactsALargeRootBatchWithoutChangingFutureCoverage) {
  constexpr size_t dimension = 14;
  constexpr size_t completed_cardinality = 6;
  std::vector<uint64_t> roots;
  for (uint64_t candidate = 1; candidate < (uint64_t{1} << dimension);
       candidate += 2) {
    if (__builtin_popcountll(static_cast<unsigned long long>(candidate)) == 7)
      roots.push_back(candidate);
  }

  const std::vector<uint64_t> compacted =
      model::g1_compact_masks(dimension, completed_cardinality, roots);
  EXPECT_LT(compacted.size(), roots.size());

  const auto covered = [](uint64_t candidate,
                          const std::vector<uint64_t> &family) {
    return std::any_of(family.begin(), family.end(), [&](uint64_t root) {
      return (root & candidate) == root;
    });
  };
  for (uint64_t candidate = 1; candidate < (uint64_t{1} << dimension);
       ++candidate) {
    const size_t cardinality = static_cast<size_t>(
        __builtin_popcountll(static_cast<unsigned long long>(candidate)));
    if (cardinality > completed_cardinality)
      EXPECT_EQ(covered(candidate, roots), covered(candidate, compacted));
  }
}

TEST(G1Test, RetiresBoundedIntervalsAboveTheirUpperCardinality) {
  EXPECT_EQ(model::g1_interval_contains(5, 0b00011, 0b01111, 0b01011, 3),
            (std::pair<bool, size_t>{true, 1}));
  EXPECT_EQ(model::g1_interval_contains(5, 0b00011, 0b01111, 0b10011, 3),
            (std::pair<bool, size_t>{false, 1}));
  EXPECT_EQ(model::g1_interval_contains(5, 0b00011, 0b01111, 0b01011, 5),
            (std::pair<bool, size_t>{false, 0}));
}

TEST(G1Test, ChecksCurvatureWhenAnyCoveringIntervalRequiresIt) {
  EXPECT_TRUE(model::g1_overlapping_intervals_check_curvature(
      5, 0b00001, 0b01111, 0b00010, 0b10111, 0b00011));
}

TEST(G1Test, ClassifiesTheFinalBoundedUpperEndpointExactly) {
  g1_diagnostics::clear();
  const matrix_integer matrix =
      symmetric_matrix(4, {10, 9, 9, -10, 10, -100, 0, 10, 0, 10});

  EXPECT_TRUE(model::g1_check_support_for_testing(matrix, {0}));
  EXPECT_NE(
      std::find(
          g1_diagnostics::events.begin(), g1_diagnostics::events.end(),
          g1_diagnostics::event{"bounded-certificate-check-curvature", 1}),
      g1_diagnostics::events.end());
}

TEST(G1Test, KeepsBoundedDickinsonIntervalsSeparateFromCeilingRoots) {
  g1_diagnostics::clear();
  EXPECT_TRUE(model::solve(symmetric_matrix(2, {2, -1, 2})));

  EXPECT_EQ(std::count(g1_diagnostics::events.begin(),
                       g1_diagnostics::events.end(),
                       g1_diagnostics::event{"bounded-certificate", 1}),
            2);
  EXPECT_EQ(std::count(g1_diagnostics::events.begin(),
                       g1_diagnostics::events.end(),
                       g1_diagnostics::event{"ceiling-certificate", 2}),
            1);
}

TEST(G1Test, BoundedCoverageSkipsRepeatedDickinsonWork) {
  g1_diagnostics::clear();
  EXPECT_TRUE(model::solve(symmetric_matrix(3, {2, -1, 0, 2, -1, 2})));

  EXPECT_NE(
      std::find(g1_diagnostics::events.begin(), g1_diagnostics::events.end(),
                g1_diagnostics::event{"bounded-certificate-skip-curvature", 1}),
      g1_diagnostics::events.end());
  EXPECT_NE(
      std::find(g1_diagnostics::events.begin(), g1_diagnostics::events.end(),
                g1_diagnostics::event{"bounded-covered-skip-curvature", 2}),
      g1_diagnostics::events.end());
  EXPECT_EQ(
      std::find(g1_diagnostics::events.begin(), g1_diagnostics::events.end(),
                g1_diagnostics::event{"covered-curvature-screened-good", 2}),
      g1_diagnostics::events.end());
}

TEST(G1Test, InstallsExactPairCurvatureCertificatesBeforeTraversal) {
  g1_diagnostics::clear();
  EXPECT_TRUE(model::solve(symmetric_matrix(3, {4, 5, -1, 4, -1, 4})));

  EXPECT_NE(std::find(g1_diagnostics::events.begin(),
                      g1_diagnostics::events.end(),
                      g1_diagnostics::event{"pair-curvature-certificate", 2}),
            g1_diagnostics::events.end());
}

TEST(G1Test, ChecksCurvatureAtStagedDickinsonEndpointsAndStillOptimizes) {
  g1_diagnostics::clear();
  const matrix_integer matrix =
      symmetric_matrix(4, {2, 3, 1, 0, 10, 0, 1, 0, 0, 1});

  EXPECT_TRUE(model::g1_check_support_for_testing(matrix, {0, 1}));
  EXPECT_NE(
      std::find(g1_diagnostics::events.begin(), g1_diagnostics::events.end(),
                g1_diagnostics::event{"endpoint-traditional-curvature", 3}),
      g1_diagnostics::events.end());
  EXPECT_NE(std::find(g1_diagnostics::events.begin(),
                      g1_diagnostics::events.end(),
                      g1_diagnostics::event{"halfspace-improvement", 2}),
            g1_diagnostics::events.end());
}

TEST(G1Test, FloatingGoodEndpointOnlySkipsTheOptionalLookahead) {
  g1_diagnostics::clear();
  const matrix_integer matrix =
      symmetric_matrix(4, {1, 0, 1, -1, 1, 0, -1, 10, 0, 1});

  EXPECT_TRUE(model::g1_check_support_for_testing(matrix, {0, 1}));
  EXPECT_NE(
      std::find(g1_diagnostics::events.begin(), g1_diagnostics::events.end(),
                g1_diagnostics::event{"endpoint-curvature-screened-good", 3}),
      g1_diagnostics::events.end());
  EXPECT_EQ(
      std::find(g1_diagnostics::events.begin(), g1_diagnostics::events.end(),
                g1_diagnostics::event{"endpoint-traditional-curvature", 3}),
      g1_diagnostics::events.end());
}

TEST(G1Test, PublishesDiagnosticsAndCeilingCertificateDiagnostics) {
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

TEST(G1Test, RejectsANonCopositiveMatrix) {
  EXPECT_FALSE(model::solve(symmetric_matrix(2, {1, -2, 2}),
                            model::copositivity_mode::copositive));
}

TEST(G1Test, KeepsPackedSupportsBeyondOneWord) {
  matrix_integer matrix;
  matrix.set_identity(65);
  matrix(63, 64) = integer(-2);
  matrix(64, 63) = integer(-2);
  EXPECT_FALSE(model::solve(matrix));
}

} // namespace
