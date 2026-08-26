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
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace coposit;

namespace coposit::model {
size_t improved_nbc_b9_pair_upward_count_for_testing() noexcept;
size_t improved_nbc_b9_support_upward_count_for_testing() noexcept;
size_t improved_nbc_b9_downward_count_for_testing() noexcept;
size_t improved_nbc_b9_high_float_rejection_count_for_testing() noexcept;
size_t improved_nbc_b9_optimized_certificate_count_for_testing() noexcept;
size_t improved_nbc_b9_no_hiding_actions_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices);
bool improved_nbc_b9_kkt_walk_for_testing(const matrix_integer &matrix,
                                          const std::vector<size_t> &seed);
size_t improved_nbc_b9_next_walk_gap_for_testing(size_t dimension,
                                                 size_t current_gap,
                                                 bool reached_kkt) noexcept;
bool improved_nbc_b9_floating_psd_candidate_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices);
bool improved_nbc_b9_check_support_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices);
bool improved_nbc_b9_certificate_for_testing(const matrix_integer &matrix,
                                             const std::vector<size_t> &indices,
                                             std::vector<size_t> &lower,
                                             std::vector<size_t> &upper);
size_t improved_nbc_b9_uncovered_count(
    size_t dimension, size_t cardinality,
    const std::vector<std::vector<size_t>> &upward,
    const std::vector<std::vector<size_t>> &downward,
    const std::vector<std::vector<size_t>> &exact);
size_t improved_nbc_b9_uncovered_count(
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

support support_from_mask(support_context &context, uint64_t mask) {
  support result = context.make();
  for (size_t index = 0; index < context.dimension(); ++index)
    if ((mask & (uint64_t{1} << index)) != 0)
      context.set(result, index);
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

TEST(ImprovedNbcB9Test, ResumesOneModelCallsWithoutRepeatingSupports) {
  support_context context(5);
  improved_nbc_upward_supports supports(context);
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

TEST(ImprovedNbcB9Test, LatchesCollectiveRootInconsistencyAcrossCalls) {
  support_context context(3);
  improved_nbc_upward_supports supports(context);
  supports.add_interval(support_from_mask(context, 0b000),
                        support_from_mask(context, 0b101));
  supports.start_cardinality(3);

  std::vector<size_t> indices;
  ASSERT_TRUE(supports.take_first(indices));
  EXPECT_EQ(support_mask(indices), 0b111U);

  supports.add_interval(support_from_mask(context, 0b010),
                        support_from_mask(context, 0b111));
  EXPECT_FALSE(supports.take_first(indices));
  EXPECT_TRUE(supports.all_future_covered());
}

TEST(ImprovedNbcB9Test, QueriesRetainedCoverageWithoutChangingEnumeration) {
  support_context context(4);
  improved_nbc_upward_supports supports(context);
  const support lower = support_from_mask(context, 0b0010);
  const support upper = support_from_mask(context, 0b1110);
  supports.add_interval(lower, upper);

  EXPECT_TRUE(supports.covers(support_from_mask(context, 0b0110)));
  EXPECT_FALSE(supports.covers(support_from_mask(context, 0b0011)));
  EXPECT_TRUE(supports.covers_interval(support_from_mask(context, 0b0110),
                                       support_from_mask(context, 0b1110)));
  EXPECT_FALSE(supports.covers_interval(support_from_mask(context, 0b0010),
                                        support_from_mask(context, 0b1111)));
}

TEST(ImprovedNbcB9Test, ExhaustivelyMatchesSingleIntervalCoverage) {
  for (size_t dimension = 1; dimension <= 5; ++dimension) {
    const uint64_t end = uint64_t{1} << dimension;
    for (uint64_t lower = 0; lower < end; ++lower) {
      for (uint64_t upper = lower; upper < end; ++upper) {
        if ((lower & ~upper) != 0)
          continue;
        for (size_t cardinality = 1; cardinality <= dimension; ++cardinality) {
          support_context context(dimension);
          improved_nbc_upward_supports supports(context);
          supports.add_interval(support_from_mask(context, lower),
                                support_from_mask(context, upper));
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

TEST(ImprovedNbcB9Test, ExhaustivelyMatchesIntervalsAddedBetweenModelCalls) {
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
            support_context context(dimension);
            improved_nbc_upward_supports supports(context);
            supports.add_interval(support_from_mask(context, first_lower),
                                  support_from_mask(context, first_upper));
            supports.start_cardinality(cardinality);
            std::vector<bool> seen(end, false);
            std::vector<size_t> indices;
            if (supports.take_first(indices))
              seen[support_mask(indices)] = true;

            supports.add_interval(support_from_mask(context, second_lower),
                                  support_from_mask(context, second_upper));
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

TEST(ImprovedNbcB9Test,
     ProcessesTheSeedOnceAndDefersItsCertificateUntilAfterTheWalk) {
  improved_nbc_b9_diagnostics::clear();
  const auto classification =
      model::classify(symmetric_matrix(3, {100, 0, 2, 100, 2, 1}));

  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
  EXPECT_EQ(model::improved_nbc_b9_pair_upward_count_for_testing(), 0U);
  EXPECT_EQ(model::improved_nbc_b9_support_upward_count_for_testing(), 0U);
  const auto walk = std::find_if(
      improved_nbc_b9_diagnostics::events.begin(),
      improved_nbc_b9_diagnostics::events.end(),
      [](const auto &event) { return event.name == "walk_downward"; });
  ASSERT_NE(walk, improved_nbc_b9_diagnostics::events.end());
  const auto low =
      std::find(improved_nbc_b9_diagnostics::events.begin(),
                improved_nbc_b9_diagnostics::events.end(),
                improved_nbc_b9_diagnostics::event{"process_low", 1});
  ASSERT_NE(low, improved_nbc_b9_diagnostics::events.end());
  EXPECT_LT(low, walk);
  const auto dickinson =
      std::find(improved_nbc_b9_diagnostics::events.begin(),
                improved_nbc_b9_diagnostics::events.end(),
                improved_nbc_b9_diagnostics::event{"dickinson", 1});
  ASSERT_NE(dickinson, improved_nbc_b9_diagnostics::events.end());
  EXPECT_LT(walk, dickinson);
  EXPECT_EQ(std::count(improved_nbc_b9_diagnostics::events.begin(),
                       improved_nbc_b9_diagnostics::events.end(),
                       improved_nbc_b9_diagnostics::event{"process_low", 1}),
            1);
}

TEST(ImprovedNbcB9Test, RecordsChronologicalCertificatesForTheRenderer) {
  std::ostringstream ignored_output;
  {
    diagnostics::reporter reporter(false, ignored_output, true, true);
    const auto classification =
        model::classify(symmetric_matrix(3, {100, 0, 2, 100, 2, 1}));
    EXPECT_TRUE(classification.is_copositive);
    EXPECT_TRUE(classification.is_strictly_copositive);
  }

  const std::string history = diagnostics::detail::load_diagnostics();
  const size_t step = history.find("event=heuristic_walk_step sequence=");
  const size_t terminal =
      history.find("event=heuristic_walk_terminal_factorization sequence=");
  const size_t downward =
      history.find("frontier=walk kind=walk_strict_face_minimum source=[");
  const size_t dickinson = history.find("frontier=low kind=dickinson source=[");
  ASSERT_NE(step, std::string::npos) << history;
  ASSERT_NE(terminal, std::string::npos) << history;
  ASSERT_NE(dickinson, std::string::npos) << history;
  ASSERT_NE(downward, std::string::npos) << history;
  EXPECT_LT(step, terminal);
  EXPECT_LT(terminal, downward);
  EXPECT_LT(downward, dickinson);
  const size_t step_end = history.find('\n', step);
  EXPECT_LT(history.find("walk=1 step=1", step), step_end);
  EXPECT_LT(history.find("floating_factorization=singleton", step), step_end);
  EXPECT_LT(history.find("exact_factorization=singleton", step), step_end);
  EXPECT_LT(history.find("exact_positive_inertia=0", step), step_end);
  EXPECT_LT(history.find("exact_negative_inertia=0", step), step_end);
  EXPECT_LT(history.find("curvature_filter=positive_definite", step), step_end);
  EXPECT_LT(history.find("jitter_draws=[]", step), step_end);
  EXPECT_LT(history.find("rejected_empty=0", step), step_end);
  EXPECT_LT(history.find("next=none", step), step_end);
  EXPECT_LT(history.find("outcome=kkt_new", step), step_end);
  const size_t terminal_end = history.find('\n', terminal);
  EXPECT_LT(history.find("matrix=principal", terminal), terminal_end);
  EXPECT_LT(history.find("exact_checked=yes", terminal), terminal_end);
  EXPECT_LT(history.find("exact_nullity=0", terminal), terminal_end);
  EXPECT_LT(history.find("exact_positive_inertia=1", terminal), terminal_end);
  EXPECT_LT(history.find("exact_negative_inertia=0", terminal), terminal_end);
  EXPECT_LT(history.find("exact_positive_definite=yes", terminal),
            terminal_end);
  EXPECT_NE(history.find("coverage=interval lower=[", dickinson),
            std::string::npos);
  EXPECT_NE(history.find("coverage=downward lower=[] upper=[", downward),
            std::string::npos);
  const size_t downward_end = history.find('\n', downward);
  const size_t floating_checked =
      history.find("floating_checked=yes", downward);
  EXPECT_NE(floating_checked, std::string::npos);
  EXPECT_LT(floating_checked, downward_end);
}

TEST(ImprovedNbcB9Test,
     FloatingFilterOnlyProposesExactPositiveSemidefinitenessChecks) {
  EXPECT_TRUE(model::improved_nbc_b9_floating_psd_candidate_for_testing(
      symmetric_matrix(3, {2, 0, 0, 3, 0, 4}), {0, 1, 2}));
  EXPECT_TRUE(model::improved_nbc_b9_floating_psd_candidate_for_testing(
      symmetric_matrix(3, {5, -1, 2, 5, 2, 2}), {0, 1, 2}));
  EXPECT_FALSE(model::improved_nbc_b9_floating_psd_candidate_for_testing(
      symmetric_matrix(3, {1, 0, 0, -1, 0, 1}), {0, 1, 2}));
  EXPECT_FALSE(model::improved_nbc_b9_floating_psd_candidate_for_testing(
      symmetric_matrix(2, {0, 1, 0}), {0, 1}));
}

TEST(ImprovedNbcB9Test, FloatingFilterUsesTheSelectedSubmatrixScale) {
  const matrix_integer matrix =
      symmetric_matrix(3, {1'000'000'000'000'000'000L, 0, 0, 1, 0, 1});
  EXPECT_TRUE(model::improved_nbc_b9_floating_psd_candidate_for_testing(
      matrix, {1, 2}));
}

TEST(ImprovedNbcB9Test, UsesOnlyTheExactNoHidingWalkActions) {
  constexpr size_t upward = 1;
  constexpr size_t stationary_downward = 2;
  constexpr size_t principal_downward = 4;

  EXPECT_EQ(model::improved_nbc_b9_no_hiding_actions_for_testing(
                symmetric_matrix(3, {1, -2, 0, 1, 0, 0}), {0, 1, 2}),
            upward);

  matrix_integer identity;
  identity.set_identity(3);
  EXPECT_EQ(
      model::improved_nbc_b9_no_hiding_actions_for_testing(identity, {0, 1, 2}),
      stationary_downward | principal_downward);

  EXPECT_EQ(model::improved_nbc_b9_no_hiding_actions_for_testing(
                symmetric_matrix(2, {1, -1, 1}), {0, 1}),
            stationary_downward);
  EXPECT_EQ(model::improved_nbc_b9_no_hiding_actions_for_testing(
                symmetric_matrix(2, {1, 2, 5}), {0, 1}),
            principal_downward);

  EXPECT_EQ(model::improved_nbc_b9_no_hiding_actions_for_testing(
                symmetric_matrix(3, {1, 1, 1, 1, 1, 1}), {0, 1, 2}),
            0U);
  EXPECT_EQ(model::improved_nbc_b9_no_hiding_actions_for_testing(
                symmetric_matrix(2, {1, -2, 1}), {0, 1}),
            0U);
}

TEST(ImprovedNbcB9Test, ContinuesExactlyAfterAFloatingKktDisagreement) {
  improved_nbc_b9_diagnostics::clear();
  const matrix_integer matrix = symmetric_matrix(
      3, {4503599627370522, 4503599627370516, 4503599627370514,
          4503599627370485, 4503599627370513, 4503599627370493});

  std::ostringstream ignored_output;
  {
    diagnostics::reporter reporter(false, ignored_output, true, true);
    EXPECT_TRUE(model::improved_nbc_b9_kkt_walk_for_testing(matrix, {0, 1, 2}));
  }
  const auto exact = std::find_if(
      improved_nbc_b9_diagnostics::events.begin(),
      improved_nbc_b9_diagnostics::events.end(), [](const auto &event) {
        return event.name == "walk_exact_continuation";
      });
  std::ostringstream events;
  for (const auto &event : improved_nbc_b9_diagnostics::events)
    events << event.name << ':' << event.cardinality << ' ';
  EXPECT_NE(exact, improved_nbc_b9_diagnostics::events.end()) << events.str();

  const std::string history = diagnostics::detail::load_diagnostics();
  const size_t first = history.find("event=heuristic_walk_step sequence=");
  ASSERT_NE(first, std::string::npos) << history;
  const size_t first_end = history.find('\n', first);
  EXPECT_LT(history.find("walk=1 step=1", first), first_end);
  EXPECT_LT(history.find("outcome=continue_floating", first), first_end)
      << history;
  const size_t second =
      history.find("event=heuristic_walk_step sequence=", first_end);
  ASSERT_NE(second, std::string::npos) << history;
  const size_t second_end = history.find('\n', second);
  EXPECT_LT(history.find("walk=1 step=2", second), second_end);
  EXPECT_LT(history.find("arithmetic=floating_exact", second), second_end)
      << history;
  const size_t third =
      history.find("event=heuristic_walk_step sequence=", second_end);
  ASSERT_NE(third, std::string::npos) << history;
  const size_t third_end = history.find('\n', third);
  EXPECT_LT(history.find("walk=1 step=3", third), third_end);
  EXPECT_LT(history.find("outcome=step_limit", third), third_end) << history;
  EXPECT_EQ(history.find("event=heuristic_walk_step sequence=", third_end),
            std::string::npos)
      << history;
}

TEST(ImprovedNbcB9Test, OnlyAKktEndpointResetsTheWalkBackoff) {
  EXPECT_EQ(model::improved_nbc_b9_next_walk_gap_for_testing(35, 35, false),
            70U);
  EXPECT_EQ(model::improved_nbc_b9_next_walk_gap_for_testing(35, 70, false),
            140U);
  EXPECT_EQ(model::improved_nbc_b9_next_walk_gap_for_testing(35, 140, true),
            35U);
  EXPECT_EQ(model::improved_nbc_b9_next_walk_gap_for_testing(
                35, std::numeric_limits<size_t>::max(), false),
            std::numeric_limits<size_t>::max());
}

TEST(ImprovedNbcB9Test,
     UsesConsistentAllOnesSystemToPruneSingularPsdSupportDownward) {
  improved_nbc_b9_diagnostics::clear();
  const auto classification =
      model::classify(symmetric_matrix(4, {3, -1, 1, 1, 3, 1, 1, 3, -1, 3}));

  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
  EXPECT_EQ(model::improved_nbc_b9_downward_count_for_testing(), 1U);
  EXPECT_NE(std::find(improved_nbc_b9_diagnostics::events.begin(),
                      improved_nbc_b9_diagnostics::events.end(),
                      improved_nbc_b9_diagnostics::event{"downward", 4}),
            improved_nbc_b9_diagnostics::events.end());
}

TEST(ImprovedNbcB9Test, BuildsAnExactDickinsonIntervalOnTheLowPath) {
  matrix_integer identity;
  identity.set_identity(3);
  std::vector<size_t> lower;
  std::vector<size_t> upper;

  EXPECT_TRUE(model::improved_nbc_b9_certificate_for_testing(identity, {0},
                                                             lower, upper));
  EXPECT_EQ(lower, std::vector<size_t>{0});
  EXPECT_EQ(upper, (std::vector<size_t>{0, 1, 2}));
}

TEST(ImprovedNbcB9Test, RetainsTheExactHalfspaceRayOptimization) {
  const matrix_integer matrix =
      symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14});
  bool improved = false;
  for (uint64_t mask = 1; mask < 16 && !improved; ++mask) {
    std::vector<size_t> indices;
    for (size_t index = 0; index < 4; ++index)
      if ((mask & (uint64_t{1} << index)) != 0)
        indices.push_back(index);
    (void)model::improved_nbc_b9_check_support_for_testing(matrix, indices);
    improved =
        model::improved_nbc_b9_optimized_certificate_count_for_testing() > 0;
  }
  EXPECT_TRUE(improved);
}

TEST(ImprovedNbcB9Test, StoresDickinsonIntervalsInTheBufferedNbcState) {
  const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011},
                                                             {0b100, 0b110}};
  EXPECT_EQ(model::improved_nbc_b9_uncovered_count(3, 1, intervals), 1U);
  EXPECT_EQ(model::improved_nbc_b9_uncovered_count(3, 2, intervals), 1U);

  const std::vector<std::pair<uint64_t, uint64_t>> padded_network_interval{
      {0b00001, 0b01111}};
  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(5, 2, padded_network_interval),
      7U);

  const std::vector<std::pair<uint64_t, uint64_t>> singular_pair_interval{
      {0b00011, 0b01111}};
  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(5, 2, singular_pair_interval), 9U);
}

TEST(ImprovedNbcB9Test, EncodesUpwardDownwardAndExactSupportBlocks) {
  const std::vector<std::vector<size_t>> none;
  const std::vector<std::vector<size_t>> support_01{{0, 1}};

  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(3, 1, support_01, none, none), 3U);
  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(3, 2, support_01, none, none), 2U);
  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(3, 3, support_01, none, none), 0U);

  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(3, 1, none, support_01, none), 1U);
  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(3, 2, none, support_01, none), 2U);
  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(3, 3, none, support_01, none), 1U);

  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(3, 1, none, none, support_01), 3U);
  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(3, 2, none, none, support_01), 2U);
  EXPECT_EQ(
      model::improved_nbc_b9_uncovered_count(3, 3, none, none, support_01), 1U);
}

TEST(ImprovedNbcB9Test, PreservesExactCombinedClassifications) {
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

TEST(ImprovedNbcB9Test, DoesNotRepeatSupportsAcrossCompactedCardinalityLayers) {
  const auto classification = model::classify(
      symmetric_matrix(5, {1, 2, 0, -2, 4, 4, 3, 0, -1, 1, 3, 2, 5, -3, 5}));
  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
}

TEST(ImprovedNbcB9Test, ExhaustivelyClassifiesSmallTwoByTwoIntegerMatrices) {
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

TEST(ImprovedNbcB9Test, MatchesTheIndependentExactThreeByThreeCriterion) {
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

TEST(ImprovedNbcB9Test, AppliesPairCurvaturePrepass) {
  const auto pair_excluded = model::classify(symmetric_matrix(2, {1, 2, 1}));
  EXPECT_TRUE(pair_excluded.is_copositive);
  EXPECT_TRUE(pair_excluded.is_strictly_copositive);
  EXPECT_EQ(model::improved_nbc_b9_pair_upward_count_for_testing(), 1U);
}

} // namespace
