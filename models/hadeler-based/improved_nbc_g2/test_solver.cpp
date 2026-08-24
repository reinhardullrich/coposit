#include <coposit/diagnostics.hpp>
#include <coposit/improved_nbc_upward_supports.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>

#include "source_diagnostics.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
size_t improved_nbc_g2_pair_upward_count_for_testing() noexcept;
size_t improved_nbc_g2_support_upward_count_for_testing() noexcept;
size_t improved_nbc_g2_downward_count_for_testing() noexcept;
size_t improved_nbc_g2_high_float_rejection_count_for_testing() noexcept;
size_t improved_nbc_g2_optimized_certificate_count_for_testing() noexcept;
size_t improved_nbc_g2_closed_cone_attempt_count_for_testing() noexcept;
size_t improved_nbc_g2_closed_cone_feasible_count_for_testing() noexcept;
size_t improved_nbc_g2_closed_cone_extension_count_for_testing() noexcept;
size_t improved_nbc_g2_closed_cone_upper_gain_for_testing() noexcept;
size_t improved_nbc_g2_closed_cone_exact_rejection_count_for_testing() noexcept;
bool improved_nbc_g2_floating_psd_candidate_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices);
bool improved_nbc_g2_check_support_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices);
bool improved_nbc_g2_certificate_for_testing(const matrix_integer &matrix,
                                             const std::vector<size_t> &indices,
                                             support &lower, support &upper);
size_t improved_nbc_g2_uncovered_count(
    size_t dimension, size_t cardinality,
    const std::vector<std::vector<size_t>> &upward,
    const std::vector<std::vector<size_t>> &downward,
    const std::vector<std::vector<size_t>> &exact);
size_t improved_nbc_g2_uncovered_count(
    size_t dimension, size_t cardinality,
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

support support_from_mask(size_t dimension, uint64_t mask) {
  support result(dimension);
  for (size_t index = 0; index < dimension; ++index)
    if ((mask & (uint64_t{1} << index)) != 0)
      result.set(index);
  return result;
}

uint64_t support_mask(const std::vector<size_t> &indices) {
  uint64_t result = 0;
  for (const size_t index : indices)
    result |= uint64_t{1} << index;
  return result;
}

bool interval_covers(uint64_t mask, uint64_t lower, uint64_t upper) {
  return (lower & ~mask) == 0 && (mask & ~upper) == 0;
}

TEST(ImprovedNbcG2Test, ResumesOneModelCallsWithoutRepeatingSupports) {
  improved_nbc_upward_supports supports(5);
  supports.start_cardinality(2);
  std::vector<bool> seen(uint64_t{1} << 5, false);
  std::vector<size_t> indices;
  size_t count = 0;
  while (supports.take_first(indices)) {
    ASSERT_EQ(indices.size(), 2U);
    const uint64_t mask = support_mask(indices);
    ASSERT_FALSE(seen[mask]);
    seen[mask] = true;
    ++count;
  }
  EXPECT_EQ(count, 10U);
}

TEST(ImprovedNbcG2Test, LatchesCollectiveRootInconsistencyAcrossCalls) {
  improved_nbc_upward_supports supports(3);
  supports.add_interval(support_from_mask(3, 0b000),
                        support_from_mask(3, 0b101));
  supports.start_cardinality(3);

  std::vector<size_t> indices;
  ASSERT_TRUE(supports.take_first(indices));
  EXPECT_EQ(support_mask(indices), 0b111U);

  supports.add_interval(support_from_mask(3, 0b010),
                        support_from_mask(3, 0b111));
  EXPECT_FALSE(supports.take_first(indices));
  EXPECT_TRUE(supports.all_future_covered());
}

TEST(ImprovedNbcG2Test, ExhaustivelyMatchesSingleIntervalCoverage) {
  for (size_t dimension = 1; dimension <= 5; ++dimension) {
    const uint64_t end = uint64_t{1} << dimension;
    for (uint64_t lower = 0; lower < end; ++lower) {
      for (uint64_t upper = lower; upper < end; ++upper) {
        if ((lower & ~upper) != 0)
          continue;
        for (size_t cardinality = 1; cardinality <= dimension; ++cardinality) {
          improved_nbc_upward_supports supports(dimension);
          supports.add_interval(support_from_mask(dimension, lower),
                                support_from_mask(dimension, upper));
          supports.start_cardinality(cardinality);
          std::vector<bool> seen(end, false);
          std::vector<size_t> indices;
          while (supports.take_first(indices)) {
            const uint64_t mask = support_mask(indices);
            ASSERT_EQ(indices.size(), cardinality);
            ASSERT_FALSE(interval_covers(mask, lower, upper));
            ASSERT_FALSE(seen[mask]);
            seen[mask] = true;
          }
          for (uint64_t mask = 1; mask < end; ++mask) {
            if (static_cast<size_t>(__builtin_popcountll(mask)) != cardinality)
              continue;
            EXPECT_EQ(seen[mask], !interval_covers(mask, lower, upper));
          }
        }
      }
    }
  }
}

TEST(ImprovedNbcG2Test, ExhaustivelyMatchesIntervalsAddedBetweenModelCalls) {
  constexpr size_t dimension = 3;
  constexpr uint64_t end = uint64_t{1} << dimension;
  for (uint64_t first_lower = 0; first_lower < end; ++first_lower) {
    for (uint64_t first_upper = first_lower; first_upper < end; ++first_upper) {
      if ((first_lower & ~first_upper) != 0)
        continue;
      for (uint64_t second_lower = 0; second_lower < end; ++second_lower) {
        for (uint64_t second_upper = second_lower; second_upper < end;
             ++second_upper) {
          if ((second_lower & ~second_upper) != 0)
            continue;
          for (size_t cardinality = 1; cardinality <= dimension;
               ++cardinality) {
            improved_nbc_upward_supports supports(dimension);
            supports.add_interval(support_from_mask(dimension, first_lower),
                                  support_from_mask(dimension, first_upper));
            supports.start_cardinality(cardinality);
            std::vector<bool> seen(end, false);
            std::vector<size_t> indices;
            if (supports.take_first(indices))
              seen[support_mask(indices)] = true;

            supports.add_interval(support_from_mask(dimension, second_lower),
                                  support_from_mask(dimension, second_upper));
            while (supports.take_first(indices)) {
              const uint64_t mask = support_mask(indices);
              ASSERT_EQ(indices.size(), cardinality);
              ASSERT_FALSE(interval_covers(mask, first_lower, first_upper));
              ASSERT_FALSE(interval_covers(mask, second_lower, second_upper));
              ASSERT_FALSE(seen[mask]);
              seen[mask] = true;
            }
            for (uint64_t mask = 1; mask < end; ++mask) {
              if (static_cast<size_t>(__builtin_popcountll(mask)) !=
                  cardinality)
                continue;
              if (!interval_covers(mask, first_lower, first_upper) &&
                  !interval_covers(mask, second_lower, second_upper))
                EXPECT_TRUE(seen[mask]);
            }
          }
        }
      }
    }
  }
}

TEST(ImprovedNbcG2Test, AlternatesFromOneLowSupportToTheNextOpenHighSupport) {
  improved_nbc_g2_diagnostics::clear();
  const auto classification =
      model::classify(symmetric_matrix(3, {2, 0, 0, 2, 0, 2}));

  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
  EXPECT_EQ(model::improved_nbc_g2_pair_upward_count_for_testing(), 0U);
  EXPECT_EQ(model::improved_nbc_g2_support_upward_count_for_testing(), 0U);
  EXPECT_EQ(model::improved_nbc_g2_downward_count_for_testing(), 1U);
  const auto low =
      std::find(improved_nbc_g2_diagnostics::events.begin(),
                improved_nbc_g2_diagnostics::events.end(),
                improved_nbc_g2_diagnostics::event{"process_low", 1});
  ASSERT_NE(low, improved_nbc_g2_diagnostics::events.end());
  const auto high = std::find_if(
      std::next(low), improved_nbc_g2_diagnostics::events.end(),
      [](const auto &event) {
        return event.name == "process_low" || event.name == "process_high";
      });
  ASSERT_NE(high, improved_nbc_g2_diagnostics::events.end());
  EXPECT_EQ(high->name, "process_high");
  EXPECT_NE(std::find(improved_nbc_g2_diagnostics::events.begin(),
                      improved_nbc_g2_diagnostics::events.end(),
                      improved_nbc_g2_diagnostics::event{"dickinson", 1}),
            improved_nbc_g2_diagnostics::events.end());
}

TEST(ImprovedNbcG2Test, RecordsChronologicalCertificatesForTheRenderer) {
  std::ostringstream ignored_output;
  {
    diagnostics::reporter reporter(false, ignored_output, true, true);
    const auto classification =
        model::classify(symmetric_matrix(3, {2, 0, 0, 2, 0, 2}));
    EXPECT_TRUE(classification.is_copositive);
    EXPECT_TRUE(classification.is_strictly_copositive);
  }

  const std::string history = diagnostics::detail::load_diagnostics();
  const size_t dickinson =
      history.find("event=certificate sequence=1 model=improved_nbc_g2 n=3 "
                   "frontier=low kind=dickinson source=[");
  const size_t downward =
      history.find("event=certificate sequence=2 model=improved_nbc_g2 n=3 "
                   "frontier=high kind=positive_definite source=[");
  ASSERT_NE(dickinson, std::string::npos) << history;
  ASSERT_NE(downward, std::string::npos) << history;
  EXPECT_LT(dickinson, downward);
  EXPECT_NE(history.find("coverage=interval lower=[", dickinson),
            std::string::npos);
  EXPECT_NE(history.find("coverage=downward lower=[] upper=[", downward),
            std::string::npos);
}

TEST(ImprovedNbcG2Test, UsesTheFloatingFilterBeforeExactHighFrontierWork) {
  improved_nbc_g2_diagnostics::clear();
  std::ostringstream ignored_output;
  {
    diagnostics::reporter reporter(false, ignored_output, true, true);
    (void)model::classify(symmetric_matrix(3, {1, -4, -2, 1, 0, 1}));
  }

  const auto high =
      std::find(improved_nbc_g2_diagnostics::events.begin(),
                improved_nbc_g2_diagnostics::events.end(),
                improved_nbc_g2_diagnostics::event{"process_high", 3});
  ASSERT_NE(high, improved_nbc_g2_diagnostics::events.end());
  ASSERT_NE(std::next(high), improved_nbc_g2_diagnostics::events.end());
  EXPECT_EQ(*std::next(high),
            (improved_nbc_g2_diagnostics::event{"high_float_reject", 3}));
  EXPECT_GT(model::improved_nbc_g2_high_float_rejection_count_for_testing(),
            0U);
  EXPECT_NE(diagnostics::detail::load_diagnostics().find(
                "model=improved_nbc_g2 n=3 frontier=high source=[1,2,3] "
                "floating_checked=yes exact_checked=no"),
            std::string::npos);
}

TEST(ImprovedNbcG2Test,
     FloatingFilterOnlyProposesExactPositiveSemidefinitenessChecks) {
  EXPECT_TRUE(model::improved_nbc_g2_floating_psd_candidate_for_testing(
      symmetric_matrix(3, {2, 0, 0, 3, 0, 4}), {0, 1, 2}));
  EXPECT_TRUE(model::improved_nbc_g2_floating_psd_candidate_for_testing(
      symmetric_matrix(3, {5, -1, 2, 5, 2, 2}), {0, 1, 2}));
  EXPECT_FALSE(model::improved_nbc_g2_floating_psd_candidate_for_testing(
      symmetric_matrix(3, {1, 0, 0, -1, 0, 1}), {0, 1, 2}));
  EXPECT_FALSE(model::improved_nbc_g2_floating_psd_candidate_for_testing(
      symmetric_matrix(2, {0, 1, 0}), {0, 1}));
}

TEST(ImprovedNbcG2Test, FloatingFilterUsesTheSelectedSubmatrixScale) {
  const matrix_integer matrix =
      symmetric_matrix(3, {1'000'000'000'000'000'000L, 0, 0, 1, 0, 1});
  EXPECT_TRUE(model::improved_nbc_g2_floating_psd_candidate_for_testing(
      matrix, {1, 2}));
}

TEST(ImprovedNbcG2Test,
     UsesConsistentAllOnesSystemToPruneSingularPsdSupportDownward) {
  improved_nbc_g2_diagnostics::clear();
  const auto classification =
      model::classify(symmetric_matrix(4, {3, -1, 1, 1, 3, 1, 1, 3, -1, 3}));

  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
  EXPECT_EQ(model::improved_nbc_g2_downward_count_for_testing(), 1U);
  EXPECT_NE(std::find(improved_nbc_g2_diagnostics::events.begin(),
                      improved_nbc_g2_diagnostics::events.end(),
                      improved_nbc_g2_diagnostics::event{"downward", 4}),
            improved_nbc_g2_diagnostics::events.end());
}

TEST(ImprovedNbcG2Test, BuildsAnExactDickinsonIntervalOnTheLowPath) {
  matrix_integer identity;
  identity.set_identity(3);
  support lower(3);
  support upper(3);

  EXPECT_TRUE(model::improved_nbc_g2_certificate_for_testing(identity, {0},
                                                             lower, upper));
  EXPECT_TRUE(lower.contains(0));
  EXPECT_FALSE(lower.contains(1));
  EXPECT_FALSE(lower.contains(2));
  EXPECT_TRUE(upper.contains(0));
  EXPECT_TRUE(upper.contains(1));
  EXPECT_TRUE(upper.contains(2));
}

TEST(ImprovedNbcG2Test, RetainsTheExactHalfspaceRayOptimization) {
  const matrix_integer matrix =
      symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14});
  bool improved = false;
  for (uint64_t mask = 1; mask < 16 && !improved; ++mask) {
    std::vector<size_t> indices;
    for (size_t index = 0; index < 4; ++index)
      if ((mask & (uint64_t{1} << index)) != 0)
        indices.push_back(index);
    (void)model::improved_nbc_g2_check_support_for_testing(matrix, indices);
    improved =
        model::improved_nbc_g2_optimized_certificate_count_for_testing() > 0;
  }
  EXPECT_TRUE(improved);
}

TEST(ImprovedNbcG2Test,
     ClosedConeExtensionFindsABoundaryEndpointMissedByPositiveRightHandSides) {
  const matrix_integer matrix = symmetric_matrix(3, {1, 0, -1, 1, 0, 2});
  support lower(3);
  support upper(3);

  EXPECT_TRUE(model::improved_nbc_g2_certificate_for_testing(matrix, {0, 1},
                                                             lower, upper));
  EXPECT_GT(model::improved_nbc_g2_closed_cone_attempt_count_for_testing(), 0U);
  EXPECT_GT(model::improved_nbc_g2_closed_cone_feasible_count_for_testing(),
            0U);
  EXPECT_EQ(model::improved_nbc_g2_closed_cone_extension_count_for_testing(),
            1U);
  EXPECT_EQ(model::improved_nbc_g2_closed_cone_upper_gain_for_testing(), 1U);
  EXPECT_EQ(
      model::improved_nbc_g2_closed_cone_exact_rejection_count_for_testing(),
      0U);
  EXPECT_FALSE(lower.contains(0));
  EXPECT_TRUE(lower.contains(1));
  EXPECT_TRUE(upper.contains(0));
  EXPECT_TRUE(upper.contains(1));
  EXPECT_TRUE(upper.contains(2));
}

TEST(ImprovedNbcG2Test, StoresDickinsonIntervalsInTheBufferedNbcState) {
  const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011},
                                                             {0b100, 0b110}};
  EXPECT_EQ(model::improved_nbc_g2_uncovered_count(3, 1, intervals), 1U);
  EXPECT_EQ(model::improved_nbc_g2_uncovered_count(3, 2, intervals), 1U);

  const std::vector<std::pair<uint64_t, uint64_t>> padded_network_interval{
      {0b00001, 0b01111}};
  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(5, 2, padded_network_interval),
      7U);

  const std::vector<std::pair<uint64_t, uint64_t>> singular_pair_interval{
      {0b00011, 0b01111}};
  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(5, 2, singular_pair_interval), 9U);
}

TEST(ImprovedNbcG2Test, EncodesUpwardDownwardAndExactSupportBlocks) {
  const std::vector<std::vector<size_t>> none;
  const std::vector<std::vector<size_t>> support_01{{0, 1}};

  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(3, 1, support_01, none, none), 3U);
  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(3, 2, support_01, none, none), 2U);
  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(3, 3, support_01, none, none), 0U);

  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(3, 1, none, support_01, none), 1U);
  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(3, 2, none, support_01, none), 2U);
  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(3, 3, none, support_01, none), 1U);

  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(3, 1, none, none, support_01), 3U);
  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(3, 2, none, none, support_01), 2U);
  EXPECT_EQ(
      model::improved_nbc_g2_uncovered_count(3, 3, none, none, support_01), 1U);
}

TEST(ImprovedNbcG2Test, PreservesExactCombinedClassifications) {
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

TEST(ImprovedNbcG2Test, DoesNotRepeatSupportsAcrossCompactedCardinalityLayers) {
  const auto classification = model::classify(
      symmetric_matrix(5, {1, 2, 0, -2, 4, 4, 3, 0, -1, 1, 3, 2, 5, -3, 5}));
  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
}

TEST(ImprovedNbcG2Test, ExhaustivelyClassifiesSmallTwoByTwoIntegerMatrices) {
  for (slong diagonal_0 = -2; diagonal_0 <= 2; ++diagonal_0) {
    for (slong off_diagonal = -2; off_diagonal <= 2; ++off_diagonal) {
      for (slong diagonal_1 = -2; diagonal_1 <= 2; ++diagonal_1) {
        const auto classification = model::classify(
            symmetric_matrix(2, {diagonal_0, off_diagonal, diagonal_1}));
        const slong determinant =
            diagonal_0 * diagonal_1 - off_diagonal * off_diagonal;
        const bool expected_copositive =
            diagonal_0 >= 0 && diagonal_1 >= 0 &&
            (off_diagonal >= 0 || determinant >= 0);
        const bool expected_strict = diagonal_0 > 0 && diagonal_1 > 0 &&
                                     (off_diagonal >= 0 || determinant > 0);

        SCOPED_TRACE(::testing::Message()
                     << "A=[" << diagonal_0 << ',' << off_diagonal << ';'
                     << diagonal_1 << ']');
        EXPECT_EQ(classification.is_copositive, expected_copositive);
        EXPECT_EQ(classification.is_strictly_copositive, expected_strict);
      }
    }
  }
}

TEST(ImprovedNbcG2Test, MatchesTheIndependentExactThreeByThreeCriterion) {
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

TEST(ImprovedNbcG2Test, AppliesPairCurvaturePrepass) {
  const auto pair_excluded = model::classify(symmetric_matrix(2, {1, 2, 1}));
  EXPECT_TRUE(pair_excluded.is_copositive);
  EXPECT_TRUE(pair_excluded.is_strictly_copositive);
  EXPECT_EQ(model::improved_nbc_g2_pair_upward_count_for_testing(), 1U);
}

} // namespace
