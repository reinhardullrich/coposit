#include <coposit/diagnostics.hpp>
#include <coposit/interval_supports.hpp>
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
size_t interval_supports_g3_pair_upward_count_for_testing() noexcept;
size_t interval_supports_g3_support_upward_count_for_testing() noexcept;
size_t interval_supports_g3_downward_count_for_testing() noexcept;
size_t interval_supports_g3_high_float_rejection_count_for_testing() noexcept;
size_t interval_supports_g3_optimized_certificate_count_for_testing() noexcept;
bool interval_supports_g3_floating_psd_candidate_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices);
bool interval_supports_g3_check_support_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices);
bool interval_supports_g3_certificate_for_testing(const matrix_integer &matrix,
                                             const std::vector<size_t> &indices,
                                             support &lower, support &upper);
size_t interval_supports_g3_uncovered_count(
    size_t dimension, size_t cardinality,
    const std::vector<std::vector<size_t>> &upward,
    const std::vector<std::vector<size_t>> &downward,
    const std::vector<std::vector<size_t>> &exact);
size_t interval_supports_g3_uncovered_count(
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

TEST(IntervalSupportsG3Test, ResumesOneModelCallsWithoutRepeatingSupports) {
  support_context context(5);
  interval_supports supports(context);
  supports.start_low_cardinality(2);
  std::vector<bool> seen(uint64_t{1} << 5, false);
  std::vector<size_t> indices;
  size_t count = 0;
  while (supports.take_first_low(indices)) {
    ASSERT_EQ(indices.size(), 2U);
    const uint64_t mask = support_mask(indices);
    ASSERT_FALSE(seen[mask]);
    seen[mask] = true;
    ++count;
  }
  EXPECT_EQ(count, 10U);
}

TEST(IntervalSupportsG3Test, CollectiveIntervalsExhaustTheLiveStream) {
  support_context context(3);
  interval_supports supports(context);
  supports.add_interval(support_from_mask(context, 0b000),
                        support_from_mask(context, 0b101));
  supports.start_low_cardinality(3);

  std::vector<size_t> indices;
  ASSERT_TRUE(supports.take_first_low(indices));
  EXPECT_EQ(support_mask(indices), 0b111U);

  supports.add_interval(support_from_mask(context, 0b010),
                        support_from_mask(context, 0b111));
  EXPECT_FALSE(supports.take_first_low(indices));
}

TEST(IntervalSupportsG3Test, ExhaustivelyMatchesSingleIntervalCoverage) {
  for (size_t dimension = 1; dimension <= 5; ++dimension) {
    const uint64_t end = uint64_t{1} << dimension;
    for (uint64_t lower = 0; lower < end; ++lower) {
      for (uint64_t upper = lower; upper < end; ++upper) {
        if ((lower & ~upper) != 0)
          continue;
        for (size_t cardinality = 1; cardinality <= dimension; ++cardinality) {
          support_context context(dimension);
          interval_supports supports(context);
          supports.add_interval(support_from_mask(context, lower),
                                support_from_mask(context, upper));
          supports.start_low_cardinality(cardinality);
          std::vector<bool> seen(end, false);
          std::vector<size_t> indices;
          while (supports.take_first_low(indices)) {
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

TEST(IntervalSupportsG3Test, ExhaustivelyMatchesIntervalsAddedBetweenModelCalls) {
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
            interval_supports supports(context);
            supports.add_interval(support_from_mask(context, first_lower),
                                  support_from_mask(context, first_upper));
            supports.start_low_cardinality(cardinality);
            std::vector<bool> seen(end, false);
            std::vector<size_t> indices;
            if (supports.take_first_low(indices))
              seen[support_mask(indices)] = true;

            supports.add_interval(support_from_mask(context, second_lower),
                                  support_from_mask(context, second_upper));
            while (supports.take_first_low(indices)) {
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

TEST(IntervalSupportsG3Test, AlternatesFromOneLowSupportToTheNextOpenHighSupport) {
  interval_supports_g3_diagnostics::clear();
  const auto classification =
      model::classify(symmetric_matrix(3, {2, 0, 0, 2, 0, 2}));

  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
  EXPECT_EQ(model::interval_supports_g3_pair_upward_count_for_testing(), 0U);
  EXPECT_EQ(model::interval_supports_g3_support_upward_count_for_testing(), 0U);
  EXPECT_EQ(model::interval_supports_g3_downward_count_for_testing(), 1U);
  const auto low =
      std::find(interval_supports_g3_diagnostics::events.begin(),
                interval_supports_g3_diagnostics::events.end(),
                interval_supports_g3_diagnostics::event{"process_low", 1});
  ASSERT_NE(low, interval_supports_g3_diagnostics::events.end());
  const auto high = std::find_if(
      std::next(low), interval_supports_g3_diagnostics::events.end(),
      [](const auto &event) {
        return event.name == "process_low" || event.name == "process_high";
      });
  ASSERT_NE(high, interval_supports_g3_diagnostics::events.end());
  EXPECT_EQ(high->name, "process_high");
  EXPECT_NE(std::find(interval_supports_g3_diagnostics::events.begin(),
                      interval_supports_g3_diagnostics::events.end(),
                      interval_supports_g3_diagnostics::event{"dickinson", 1}),
            interval_supports_g3_diagnostics::events.end());
}

TEST(IntervalSupportsG3Test, RecordsChronologicalCertificatesForTheRenderer) {
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
      history.find("event=certificate sequence=1 model=interval_supports_g3 n=3 "
                   "frontier=low kind=dickinson source=[");
  const size_t downward =
      history.find("event=certificate sequence=2 model=interval_supports_g3 n=3 "
                   "frontier=high kind=positive_definite source=[");
  ASSERT_NE(dickinson, std::string::npos) << history;
  ASSERT_NE(downward, std::string::npos) << history;
  EXPECT_LT(dickinson, downward);
  EXPECT_NE(history.find("coverage=interval lower=[", dickinson),
            std::string::npos);
  EXPECT_NE(history.find("coverage=downward lower=[] upper=[", downward),
            std::string::npos);
}

TEST(IntervalSupportsG3Test, UsesTheFloatingFilterBeforeExactHighFrontierWork) {
  interval_supports_g3_diagnostics::clear();
  std::ostringstream ignored_output;
  {
    diagnostics::reporter reporter(false, ignored_output, true, true);
    (void)model::classify(symmetric_matrix(3, {1, -4, -2, 1, 0, 1}));
  }

  const auto high =
      std::find(interval_supports_g3_diagnostics::events.begin(),
                interval_supports_g3_diagnostics::events.end(),
                interval_supports_g3_diagnostics::event{"process_high", 3});
  ASSERT_NE(high, interval_supports_g3_diagnostics::events.end());
  ASSERT_NE(std::next(high), interval_supports_g3_diagnostics::events.end());
  EXPECT_EQ(*std::next(high),
            (interval_supports_g3_diagnostics::event{"high_float_reject", 3}));
  EXPECT_GT(model::interval_supports_g3_high_float_rejection_count_for_testing(),
            0U);
  EXPECT_NE(diagnostics::detail::load_diagnostics().find(
                "model=interval_supports_g3 n=3 frontier=high source=[1,2,3] "
                "floating_checked=yes exact_checked=no"),
            std::string::npos);
}

TEST(IntervalSupportsG3Test,
     FloatingFilterOnlyProposesExactPositiveSemidefinitenessChecks) {
  EXPECT_TRUE(model::interval_supports_g3_floating_psd_candidate_for_testing(
      symmetric_matrix(3, {2, 0, 0, 3, 0, 4}), {0, 1, 2}));
  EXPECT_TRUE(model::interval_supports_g3_floating_psd_candidate_for_testing(
      symmetric_matrix(3, {5, -1, 2, 5, 2, 2}), {0, 1, 2}));
  EXPECT_FALSE(model::interval_supports_g3_floating_psd_candidate_for_testing(
      symmetric_matrix(3, {1, 0, 0, -1, 0, 1}), {0, 1, 2}));
  EXPECT_FALSE(model::interval_supports_g3_floating_psd_candidate_for_testing(
      symmetric_matrix(2, {0, 1, 0}), {0, 1}));
}

TEST(IntervalSupportsG3Test, FloatingFilterUsesTheSelectedSubmatrixScale) {
  const matrix_integer matrix =
      symmetric_matrix(3, {1'000'000'000'000'000'000L, 0, 0, 1, 0, 1});
  EXPECT_TRUE(model::interval_supports_g3_floating_psd_candidate_for_testing(
      matrix, {1, 2}));
}

TEST(IntervalSupportsG3Test,
     UsesConsistentAllOnesSystemToPruneSingularPsdSupportDownward) {
  interval_supports_g3_diagnostics::clear();
  const auto classification =
      model::classify(symmetric_matrix(4, {3, -1, 1, 1, 3, 1, 1, 3, -1, 3}));

  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
  EXPECT_EQ(model::interval_supports_g3_downward_count_for_testing(), 1U);
  EXPECT_NE(std::find(interval_supports_g3_diagnostics::events.begin(),
                      interval_supports_g3_diagnostics::events.end(),
                      interval_supports_g3_diagnostics::event{"downward", 4}),
            interval_supports_g3_diagnostics::events.end());
}

TEST(IntervalSupportsG3Test, BuildsAnExactDickinsonIntervalOnTheLowPath) {
  matrix_integer identity;
  identity.set_identity(3);
  support_context context(3);
  support lower = context.make();
  support upper = context.make();

  EXPECT_TRUE(model::interval_supports_g3_certificate_for_testing(identity, {0},
                                                             lower, upper));
  EXPECT_TRUE(context.contains(lower, 0));
  EXPECT_FALSE(context.contains(lower, 1));
  EXPECT_FALSE(context.contains(lower, 2));
  EXPECT_TRUE(context.contains(upper, 0));
  EXPECT_TRUE(context.contains(upper, 1));
  EXPECT_TRUE(context.contains(upper, 2));
}

TEST(IntervalSupportsG3Test, RetainsTheExactHalfspaceRayOptimization) {
  const matrix_integer matrix =
      symmetric_matrix(4, {6, -4, 5, -5, 6, -7, 5, 10, -10, 14});
  bool improved = false;
  for (uint64_t mask = 1; mask < 16 && !improved; ++mask) {
    std::vector<size_t> indices;
    for (size_t index = 0; index < 4; ++index)
      if ((mask & (uint64_t{1} << index)) != 0)
        indices.push_back(index);
    (void)model::interval_supports_g3_check_support_for_testing(matrix, indices);
    improved =
        model::interval_supports_g3_optimized_certificate_count_for_testing() > 0;
  }
  EXPECT_TRUE(improved);
}

TEST(IntervalSupportsG3Test, StoresDickinsonIntervalsInTheIntervalGenerator) {
  const std::vector<std::pair<uint64_t, uint64_t>> intervals{{0b001, 0b011},
                                                             {0b100, 0b110}};
  EXPECT_EQ(model::interval_supports_g3_uncovered_count(3, 1, intervals), 1U);
  EXPECT_EQ(model::interval_supports_g3_uncovered_count(3, 2, intervals), 1U);

  const std::vector<std::pair<uint64_t, uint64_t>> padded_network_interval{
      {0b00001, 0b01111}};
  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(5, 2, padded_network_interval),
      7U);

  const std::vector<std::pair<uint64_t, uint64_t>> singular_pair_interval{
      {0b00011, 0b01111}};
  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(5, 2, singular_pair_interval), 9U);
}

TEST(IntervalSupportsG3Test, EncodesUpwardDownwardAndExactSupportBlocks) {
  const std::vector<std::vector<size_t>> none;
  const std::vector<std::vector<size_t>> support_01{{0, 1}};

  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(3, 1, support_01, none, none), 3U);
  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(3, 2, support_01, none, none), 2U);
  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(3, 3, support_01, none, none), 0U);

  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(3, 1, none, support_01, none), 1U);
  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(3, 2, none, support_01, none), 2U);
  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(3, 3, none, support_01, none), 1U);

  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(3, 1, none, none, support_01), 3U);
  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(3, 2, none, none, support_01), 2U);
  EXPECT_EQ(
      model::interval_supports_g3_uncovered_count(3, 3, none, none, support_01), 1U);
}

TEST(IntervalSupportsG3Test, PreservesExactCombinedClassifications) {
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

TEST(IntervalSupportsG3Test, DoesNotRepeatSupportsAcrossCardinalityLayers) {
  const auto classification = model::classify(
      symmetric_matrix(5, {1, 2, 0, -2, 4, 4, 3, 0, -1, 1, 3, 2, 5, -3, 5}));
  EXPECT_TRUE(classification.is_copositive);
  EXPECT_TRUE(classification.is_strictly_copositive);
}

TEST(IntervalSupportsG3Test, ExhaustivelyClassifiesSmallTwoByTwoIntegerMatrices) {
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

TEST(IntervalSupportsG3Test, MatchesTheIndependentExactThreeByThreeCriterion) {
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

TEST(IntervalSupportsG3Test, AppliesPairCurvaturePrepass) {
  const auto pair_excluded = model::classify(symmetric_matrix(2, {1, 2, 1}));
  EXPECT_TRUE(pair_excluded.is_copositive);
  EXPECT_TRUE(pair_excluded.is_strictly_copositive);
  EXPECT_EQ(model::interval_supports_g3_pair_upward_count_for_testing(), 1U);
}

} // namespace
