#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

struct positive_ratio {
  integer numerator;
  integer denominator;
};

struct breakpoint_event {
  positive_ratio root;
  bool solution_entry = false;
  int direction_sign = 0;
};

bool ratio_less(const positive_ratio &left, const positive_ratio &right) {
  integer left_product;
  integer right_product;
  left_product.set_product(left.numerator, right.denominator);
  right_product.set_product(right.numerator, left.denominator);
  return left_product.compare(right_product) < 0;
}

bool ratio_equal(const positive_ratio &left, const positive_ratio &right) {
  return !ratio_less(left, right) && !ratio_less(right, left);
}

class floating_reduced_curvature_filter {
public:
  explicit floating_reduced_curvature_filter(size_t dimension)
      : dimension_(dimension) {}

  void prepare(const matrix_integer &matrix) {
    if (prepared_)
      return;

    matrix_.assign(dimension_ * dimension_, 0.0);
    slong maximum_exponent = std::numeric_limits<slong>::min();
    for (size_t row = 0; row < dimension_; ++row) {
      for (size_t column = 0; column <= row; ++column) {
        if (matrix(row, column).is_zero())
          continue;
        slong exponent = 0;
        static_cast<void>(matrix(row, column).to_dbl_2exp(exponent));
        maximum_exponent = std::max(maximum_exponent, exponent);
      }
    }
    if (maximum_exponent == std::numeric_limits<slong>::min()) {
      prepared_ = true;
      return;
    }

    for (size_t row = 0; row < dimension_; ++row) {
      timeout_checkpoint();
      for (size_t column = 0; column <= row; ++column) {
        const auto entry = matrix(row, column);
        if (entry.is_zero())
          continue;
        slong exponent = 0;
        const double mantissa = entry.to_dbl_2exp(exponent);
        const long double shift = static_cast<long double>(exponent) -
                                  static_cast<long double>(maximum_exponent);
        const double value =
            shift < static_cast<long double>(std::numeric_limits<int>::min())
                ? 0.0
                : std::scalbn(mantissa, static_cast<int>(shift));
        matrix_[row * dimension_ + column] = value;
        matrix_[column * dimension_ + row] = value;
      }
    }
    prepared_ = true;
  }

  bool
  looks_reduced_hessian_positive_definite(const std::vector<size_t> &indices) {
    assert(indices.size() >= 3);
    const size_t order = indices.size() - 1;
    const size_t anchor = indices.back();
    principal_.resize(order * order);
    diagonal_.resize(order);

    double scale = 0.0;
    for (size_t row = 0; row < order; ++row) {
      for (size_t column = 0; column <= row; ++column) {
        const double entry =
            matrix_[indices[row] * dimension_ + indices[column]] -
            matrix_[indices[row] * dimension_ + anchor] -
            matrix_[anchor * dimension_ + indices[column]] +
            matrix_[anchor * dimension_ + anchor];
        principal_[row * order + column] = entry;
        scale = std::max(scale, std::abs(entry));
      }
    }
    if (scale == 0.0 || !std::isfinite(scale))
      return false;

    const double tolerance = 64.0 * std::numeric_limits<double>::epsilon() *
                             scale * static_cast<double>(order);
    for (size_t column = 0; column < order; ++column) {
      timeout_checkpoint();
      double pivot = principal_[column * order + column];
      for (size_t previous = 0; previous < column; ++previous) {
        const double multiplier = principal_[column * order + previous];
        pivot -= multiplier * multiplier * diagonal_[previous];
      }
      if (!std::isfinite(pivot) || pivot <= tolerance)
        return false;
      diagonal_[column] = pivot;

      for (size_t row = column + 1; row < order; ++row) {
        double entry = principal_[row * order + column];
        for (size_t previous = 0; previous < column; ++previous) {
          entry -= principal_[row * order + previous] *
                   principal_[column * order + previous] * diagonal_[previous];
        }
        entry /= pivot;
        if (!std::isfinite(entry))
          return false;
        principal_[row * order + column] = entry;
      }
    }
    return true;
  }

private:
  size_t dimension_;
  bool prepared_ = false;
  std::vector<double> matrix_;
  std::vector<double> principal_;
  std::vector<double> diagonal_;
};

struct coverage_score {
  size_t width = 0;
  size_t upper_size = 0;
};

bool better_score(const coverage_score &candidate,
                  const coverage_score &current) noexcept {
  return candidate.upper_size > current.upper_size ||
         (candidate.upper_size == current.upper_size &&
          candidate.width > current.width);
}

bool better_ray_candidate(const coverage_score &candidate, size_t gains,
                          size_t losses, bool current_initialized,
                          const coverage_score &current, size_t current_gains,
                          size_t current_losses) noexcept {
  if (!current_initialized || better_score(candidate, current))
    return true;
  if (candidate.upper_size != current.upper_size ||
      candidate.width != current.width)
    return false;
  return gains > current_gains ||
         (gains == current_gains && losses < current_losses);
}

constexpr size_t maximum_ray_shortlist = 64;

size_t ray_shortlist_limit(size_t matrix_dimension, size_t support_dimension) {
  const size_t dimension_budget = static_cast<size_t>(
      std::ceil(3.0L * std::sqrt(static_cast<long double>(matrix_dimension))));
  return std::min({support_dimension, maximum_ray_shortlist, dimension_budget});
}

struct ray_candidate {
  positive_ratio step;
  coverage_score score;
  size_t direction = 0;
  size_t gains = 0;
  size_t losses = 0;
};

bool better_shortlist_candidate(const ray_candidate &candidate,
                                const ray_candidate &current) noexcept {
  if (candidate.gains != current.gains)
    return candidate.gains > current.gains;
  if (candidate.losses != current.losses)
    return candidate.losses < current.losses;
  if (candidate.score.upper_size != current.score.upper_size)
    return candidate.score.upper_size > current.score.upper_size;
  if (candidate.score.width != current.score.width)
    return candidate.score.width > current.score.width;
  return candidate.direction < current.direction;
}

struct pair_score {
  size_t union_gains = 0;
  size_t common_losses = 0;
  size_t total_gains = 0;
  size_t union_losses = 0;
};

struct ray_pair {
  size_t first = 0;
  size_t second = 0;
  pair_score score;
  bool initialized = false;
};

bool better_pair(const ray_pair &candidate, const ray_pair &current) noexcept {
  if (!current.initialized)
    return true;
  if (candidate.score.union_gains != current.score.union_gains)
    return candidate.score.union_gains > current.score.union_gains;
  if (candidate.score.common_losses != current.score.common_losses)
    return candidate.score.common_losses < current.score.common_losses;
  if (candidate.score.total_gains != current.score.total_gains)
    return candidate.score.total_gains > current.score.total_gains;
  if (candidate.score.union_losses != current.score.union_losses)
    return candidate.score.union_losses < current.score.union_losses;
  return std::pair{candidate.first, candidate.second} <
         std::pair{current.first, current.second};
}

uint64_t saturated_binomial(size_t n, size_t k) noexcept {
  if (k > n)
    return 0;
  k = std::min(k, n - k);
  uint64_t result = 1;
  for (size_t i = 1; i <= k; ++i) {
    size_t numerator = n - k + i;
    size_t denominator = i;
    const size_t numerator_divisor = std::gcd(numerator, denominator);
    numerator /= numerator_divisor;
    denominator /= numerator_divisor;
    const uint64_t result_divisor =
        std::gcd(result, static_cast<uint64_t>(denominator));
    result /= result_divisor;
    assert(denominator == static_cast<size_t>(result_divisor));
    if (numerator != 0 &&
        result > std::numeric_limits<uint64_t>::max() / numerator)
      return std::numeric_limits<uint64_t>::max();
    result *= numerator;
  }
  return result;
}

struct bounded_interval {
  support lower;
  support upper;
  size_t upper_size = 0;
  bool check_curvature = false;
};

enum class bounded_coverage { none, skip_curvature, check_curvature };

class bounded_interval_index {
public:
  explicit bounded_interval_index(size_t dimension)
      : dimension_(dimension), by_trigger_(dimension) {}

  void start_cardinality(size_t cardinality) {
    for (auto &bucket : by_trigger_) {
      bucket.erase(std::remove_if(bucket.begin(), bucket.end(),
                                  [&](const bounded_interval &interval) {
                                    return interval.upper_size < cardinality;
                                  }),
                   bucket.end());
    }
  }

  void add(support lower, support upper, bool check_curvature) {
    assert(!lower.empty());
    const size_t upper_size = upper.cardinality();
    assert(upper_size < dimension_);
    assert(lower.is_subset_of(upper));

    size_t trigger = lower.lowest_index();
    for (size_t bit = trigger + 1; bit < dimension_; ++bit)
      if (lower.contains(bit) &&
          by_trigger_[bit].size() < by_trigger_[trigger].size())
        trigger = bit;
    by_trigger_[trigger].push_back(
        {std::move(lower), std::move(upper), upper_size, check_curvature});
  }

  bounded_coverage coverage(const support &candidate) const noexcept {
    bounded_coverage result = bounded_coverage::none;
    for (size_t bit = 0; bit < dimension_; ++bit) {
      if (!candidate.contains(bit))
        continue;
      for (const bounded_interval &interval : by_trigger_[bit])
        if (interval.lower.is_subset_of(candidate) &&
            candidate.is_subset_of(interval.upper)) {
          if (interval.check_curvature)
            return bounded_coverage::check_curvature;
          result = bounded_coverage::skip_curvature;
        }
    }
    return result;
  }

#ifdef COPOSIT_G1_TESTING
  size_t size_for_testing() const noexcept {
    size_t result = 0;
    for (const auto &bucket : by_trigger_)
      result += bucket.size();
    return result;
  }
#endif

private:
  size_t dimension_;
  std::vector<std::vector<bounded_interval>> by_trigger_;
};

enum class extension_search_result { covered, uncovered, budget_exhausted };

class support_generator {
public:
  explicit support_generator(size_t dimension,
                             diagnostics::tracker *diagnostics = nullptr)
      : dimension_(dimension), forbidden_by_lowest_(dimension),
        partial_support_(dimension), diagnostics_(diagnostics) {}

  template <class LayerCallback, class Callback>
  bool generate(LayerCallback &&start_layer, Callback &&callback) {
    for (target_cardinality_ = 1; target_cardinality_ <= dimension_;
         ++target_cardinality_) {
      activate_pending();
      if (target_cardinality_ > 1)
        compact_completed_layers(target_cardinality_ - 1);
      start_layer(target_cardinality_);
      if (all_future_forbidden_) {
        if (diagnostics_ != nullptr)
          for (size_t remaining = target_cardinality_; remaining <= dimension_;
               ++remaining)
            diagnostics_->skip_supports(
                saturated_binomial(dimension_, remaining));
        return true;
      }
      emitted_ = false;
      if (diagnostics_ != nullptr)
        diagnostics_->support_cardinality(target_cardinality_);
      if (!generate_from(dimension_, target_cardinality_, callback))
        return false;
      if (!emitted_) {
        if (diagnostics_ != nullptr)
          for (size_t remaining = target_cardinality_ + 1;
               remaining <= dimension_; ++remaining)
            diagnostics_->skip_supports(
                saturated_binomial(dimension_, remaining));
        return true;
      }
    }
    return true;
  }

  void add_forbidden(support lower) {
    assert(!lower.empty());
    pending_forbidden_.push_back(std::move(lower));
  }

#ifdef COPOSIT_G1_TESTING
  void compact_for_testing(size_t completed_cardinality) {
    activate_pending();
    compact_completed_layers(completed_cardinality, true);
  }

  const std::vector<support> &roots_for_testing() const noexcept {
    return forbidden_;
  }
  bool all_future_forbidden_for_testing() const noexcept {
    return all_future_forbidden_;
  }
#endif

private:
  void activate_pending() {
    for (support &forbidden : pending_forbidden_) {
      const bool check_redundancy = !roots_need_normalization_ &&
                                    forbidden_.size() <= compaction_threshold();
      if (check_redundancy && is_covered(forbidden))
        continue;
      roots_need_normalization_ |= !check_redundancy;
      const size_t index = forbidden_.size();
      forbidden_.push_back(std::move(forbidden));
      forbidden_by_lowest_[forbidden_[index].lowest_index()].push_back(index);
    }
    pending_forbidden_.clear();
  }

  bool is_covered(const support &candidate) const noexcept {
    if (candidate.empty())
      return all_future_forbidden_;
    for (size_t bit = 0; bit < dimension_; ++bit) {
      if (!candidate.contains(bit))
        continue;
      for (const size_t index : forbidden_by_lowest_[bit])
        if (forbidden_[index].is_subset_of(candidate))
          return true;
    }
    return false;
  }

  bool completes_forbidden(size_t new_lowest_bit) const noexcept {
    for (const size_t index : forbidden_by_lowest_[new_lowest_bit])
      if (forbidden_[index].is_subset_of(partial_support_))
        return true;
    return false;
  }

  static size_t saturated_product(size_t left, size_t right) noexcept {
    if (left != 0 && right > std::numeric_limits<size_t>::max() / left)
      return std::numeric_limits<size_t>::max();
    return left * right;
  }

  size_t compaction_threshold() const noexcept {
    return std::max<size_t>(512, saturated_product(8, dimension_));
  }

  size_t compaction_work_budget(size_t root_count) const noexcept {
    return std::max<size_t>(
        4096, saturated_product(16, saturated_product(dimension_, root_count)));
  }

  void compact_completed_layers(size_t completed_cardinality,
                                bool force = false) {
    if (all_future_forbidden_ || forbidden_.empty() ||
        completed_cardinality >= dimension_)
      return;
    if (!force && forbidden_.size() <= compaction_threshold())
      return;

    const size_t next_cardinality = completed_cardinality + 1;
    size_t search_budget = compaction_work_budget(forbidden_.size());
    std::vector<size_t> order(forbidden_.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t left, size_t right) {
      const size_t left_size = forbidden_[left].cardinality();
      const size_t right_size = forbidden_[right].cardinality();
      return left_size != right_size ? left_size < right_size
                                     : forbidden_[left] < forbidden_[right];
    });

    std::vector<support> expanded;
    expanded.reserve(forbidden_.size());
    for (const size_t index : order) {
      expanded.push_back(forbidden_[index]);
    }

    std::vector<bool> still_expandable(expanded.size(), true);
    std::vector<size_t> bits;
    bits.reserve(dimension_);
    bool any_expansion = false;
    bool expanded_in_round = true;
    while (search_budget != 0 && expanded_in_round) {
      expanded_in_round = false;
      for (size_t index = 0; index < expanded.size(); ++index) {
        if (!still_expandable[index])
          continue;
        still_expandable[index] = false;
        expanded[index].copy_indices_to(bits);
        for (const size_t bit : bits) {
          if (search_budget == 0)
            break;
          expanded[index].reset(bit);
          const size_t checked_cardinality =
              std::max(next_cardinality, expanded[index].cardinality());
          const extension_search_result result = find_uncovered_extension(
              expanded[index], checked_cardinality, order, search_budget);
          if (result == extension_search_result::covered) {
            still_expandable[index] = true;
            any_expansion = true;
            expanded_in_round = true;
            break;
          }
          expanded[index].set(bit);
        }
        if (expanded[index].empty()) {
          all_future_forbidden_ = true;
          forbidden_.clear();
          for (auto &bucket : forbidden_by_lowest_)
            bucket.clear();
          COPOSIT_G1_DIAGNOSTICS("compaction-covered-future", next_cardinality);
          return;
        }
      }
    }
    if (!any_expansion && !roots_need_normalization_)
      return;

    std::sort(expanded.begin(), expanded.end(),
              [](const support &left, const support &right) {
                const size_t left_size = left.cardinality();
                const size_t right_size = right.cardinality();
                return left_size != right_size ? left_size < right_size
                                               : left < right;
              });
    std::vector<support> normalized;
    normalized.reserve(expanded.size());
    std::vector<std::vector<size_t>> normalized_by_bit(dimension_);
    size_t normalization_budget = compaction_work_budget(expanded.size());
    for (support &candidate : expanded) {
      bool covered = false;
      candidate.copy_indices_to(bits);
      for (const size_t bit : bits) {
        for (const size_t index : normalized_by_bit[bit]) {
          if (normalization_budget == 0)
            break;
          --normalization_budget;
          if (normalized[index].is_subset_of(candidate)) {
            covered = true;
            break;
          }
        }
        if (covered || normalization_budget == 0)
          break;
      }
      if (!normalized.empty() && candidate == normalized.back())
        continue;
      if (covered)
        continue;
      const size_t index = normalized.size();
      normalized.push_back(std::move(candidate));
      normalized_by_bit[normalized[index].lowest_index()].push_back(index);
    }

    if (normalized.size() < forbidden_.size())
      COPOSIT_G1_DIAGNOSTICS("compaction-roots", normalized.size());
    forbidden_.swap(normalized);
    rebuild_buckets();
    roots_need_normalization_ = false;
  }

  extension_search_result
  find_uncovered_extension(const support &required, size_t cardinality,
                           const std::vector<size_t> &order,
                           size_t &work_budget) {
    assert(required.cardinality() <= cardinality);
    if (forbidden_[order.front()].cardinality() > cardinality)
      return extension_search_result::uncovered;
    partial_support_.clear();
    return find_uncovered_extension_from(dimension_, cardinality, required,
                                         required.cardinality(), order,
                                         work_budget);
  }

  extension_search_result
  find_uncovered_extension_from(size_t bits_remaining, size_t needed,
                                const support &required, size_t required_needed,
                                const std::vector<size_t> &order,
                                size_t &work_budget) {
    timeout_checkpoint();
    if (work_budget == 0)
      return extension_search_result::budget_exhausted;
    --work_budget;
    if (needed == 0)
      return required_needed == 0 ? extension_search_result::uncovered
                                  : extension_search_result::covered;
    if (needed > bits_remaining || required_needed > needed)
      return extension_search_result::covered;

    const size_t bit = bits_remaining - 1;
    if (!required.contains(bit) && needed < bits_remaining) {
      const extension_search_result without_bit = find_uncovered_extension_from(
          bit, needed, required, required_needed, order, work_budget);
      if (without_bit != extension_search_result::covered)
        return without_bit;
    }

    partial_support_.set(bit);
    extension_search_result with_bit = extension_search_result::covered;
    const size_t next_required_needed =
        required_needed - static_cast<size_t>(required.contains(bit));
    bool completes = false;
    if (needed == 1 && next_required_needed == 0) {
      const extension_search_result exact =
          exact_forbidden(partial_support_, order, work_budget);
      if (exact == extension_search_result::budget_exhausted) {
        partial_support_.reset(bit);
        return exact;
      }
      completes = exact == extension_search_result::covered;
    }
    if (!completes) {
      for (const size_t index : forbidden_by_lowest_[bit]) {
        if (work_budget == 0) {
          partial_support_.reset(bit);
          return extension_search_result::budget_exhausted;
        }
        --work_budget;
        if (forbidden_[index].is_subset_of(partial_support_)) {
          completes = true;
          break;
        }
      }
    }
    if (!completes)
      with_bit = find_uncovered_extension_from(
          bit, needed - 1, required, next_required_needed, order, work_budget);
    partial_support_.reset(bit);
    return with_bit;
  }

  extension_search_result exact_forbidden(const support &candidate,
                                          const std::vector<size_t> &order,
                                          size_t &work_budget) const {
    const size_t candidate_size = candidate.cardinality();
    size_t first = 0;
    size_t count = order.size();
    while (count != 0) {
      if (work_budget == 0)
        return extension_search_result::budget_exhausted;
      --work_budget;
      const size_t step = count / 2;
      const size_t middle = first + step;
      const support &root = forbidden_[order[middle]];
      const size_t root_size = root.cardinality();
      if (root_size < candidate_size ||
          (root_size == candidate_size && root < candidate)) {
        first = middle + 1;
        count -= step + 1;
      } else {
        count = step;
      }
    }
    return first != order.size() && forbidden_[order[first]] == candidate
               ? extension_search_result::covered
               : extension_search_result::uncovered;
  }

  void rebuild_buckets() {
    for (auto &bucket : forbidden_by_lowest_)
      bucket.clear();
    for (size_t index = 0; index < forbidden_.size(); ++index)
      forbidden_by_lowest_[forbidden_[index].lowest_index()].push_back(index);
  }

  template <class Callback>
  bool generate_from(size_t bits_remaining, size_t needed, Callback &callback) {
    timeout_checkpoint();
    if (needed == 0) {
      emitted_ = true;
      if (diagnostics_ != nullptr)
        diagnostics_->visit_support();
      return callback(partial_support_, target_cardinality_);
    }
    if (needed > bits_remaining)
      return true;

    const size_t bit = bits_remaining - 1;
    if (needed < bits_remaining && !generate_from(bit, needed, callback))
      return false;

    partial_support_.set(bit);
    bool keep_going = true;
    if (completes_forbidden(bit)) {
      if (diagnostics_ != nullptr)
        diagnostics_->skip_supports(saturated_binomial(bit, needed - 1));
    } else {
      keep_going = generate_from(bit, needed - 1, callback);
    }
    partial_support_.reset(bit);
    return keep_going;
  }

  size_t dimension_;
  std::vector<support> forbidden_;
  std::vector<std::vector<size_t>> forbidden_by_lowest_;
  std::vector<support> pending_forbidden_;
  support partial_support_;
  diagnostics::tracker *diagnostics_;
  size_t target_cardinality_ = 0;
  bool emitted_ = false;
  bool all_future_forbidden_ = false;
  bool roots_need_normalization_ = false;
};

class dickinson_checker {
public:
  dickinson_checker(size_t dimension, copositivity_mode mode)
      : factorization_(dimension), endpoint_factorization_(dimension),
        floating_filter_(dimension), product_(dimension),
        shortlist_limit_(ray_shortlist_limit(dimension, dimension)),
        mode_(mode), diagnostics_(diagnostics::metric::support, dimension),
        supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr),
        intervals_(dimension) {
    indices_.reserve(dimension);
    endpoint_indices_.reserve(dimension);
    for (auto &endpoint : checked_endpoint_indices_)
      endpoint.reserve(dimension);
    ray_shortlist_.reserve(shortlist_limit_);
    shortlist_uppers_.reserve(shortlist_limit_);
    for (size_t index = 0; index < shortlist_limit_; ++index)
      shortlist_uppers_.emplace_back(dimension);
  }

  dickinson_checker(size_t dimension,
                    copositivity_classification &classification)
      : factorization_(dimension), endpoint_factorization_(dimension),
        floating_filter_(dimension), product_(dimension),
        shortlist_limit_(ray_shortlist_limit(dimension, dimension)),
        mode_(copositivity_mode::copositive), classification_(&classification),
        diagnostics_(diagnostics::metric::support, dimension),
        supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr),
        intervals_(dimension) {
    indices_.reserve(dimension);
    endpoint_indices_.reserve(dimension);
    for (auto &endpoint : checked_endpoint_indices_)
      endpoint.reserve(dimension);
    ray_shortlist_.reserve(shortlist_limit_);
    shortlist_uppers_.reserve(shortlist_limit_);
    for (size_t index = 0; index < shortlist_limit_; ++index)
      shortlist_uppers_.emplace_back(dimension);
  }

  bool check(const matrix_integer &matrix) {
    install_pair_curvature_certificates(matrix);
    const bool result = supports_.generate(
        [&](size_t cardinality) { intervals_.start_cardinality(cardinality); },
        [&](const support &current, size_t cardinality) {
          timeout_checkpoint();
          current.copy_indices_to(indices_);
          const bounded_coverage coverage = intervals_.coverage(current);
          if (coverage == bounded_coverage::check_curvature) {
            COPOSIT_G1_DIAGNOSTICS("bounded-covered", cardinality);
            COPOSIT_G1_DIAGNOSTICS("bounded-covered-check-curvature",
                                   cardinality);
            return try_covered_support_curvature(matrix);
          }
          if (coverage == bounded_coverage::skip_curvature) {
            COPOSIT_G1_DIAGNOSTICS("bounded-covered", cardinality);
            COPOSIT_G1_DIAGNOSTICS("bounded-covered-skip-curvature",
                                   cardinality);
            return true;
          }
          COPOSIT_G1_DIAGNOSTICS("process", cardinality);
          return process_subset(matrix);
        });
    diagnostics_.finish();
    return result;
  }

#ifdef COPOSIT_G1_TESTING
  bool check_support_for_testing(const matrix_integer &matrix,
                                 const std::vector<size_t> &indices) {
    indices_ = indices;
    return process_subset(matrix);
  }
#endif

private:
  enum class endpoint_stage : size_t { traditional, halfspace, rays };

  bool try_covered_support_curvature(const matrix_integer &matrix) {
    if (indices_.size() < 3)
      return true;
    floating_filter_.prepare(matrix);
    if (floating_filter_.looks_reduced_hessian_positive_definite(indices_)) {
      COPOSIT_G1_DIAGNOSTICS("covered-curvature-screened-good",
                             indices_.size());
      return true;
    }

    diagnostics_.secondary();
    principal_.resize(indices_.size(), indices_.size());
    copy_principal(matrix, indices_, principal_);
    const bool singular = factorization_.factorize_inplace(principal_) == 0;
    if (singular && diagnostics_.active())
      diagnostics_.singular_support(indices_.size() - factorization_.rank());

    bool positive_definite = false;
    if (!singular) {
      positive_definite = factorization_.is_positive_definite();
      if (!positive_definite && factorization_.negative_inertia() == 1) {
        solution_.resize(indices_.size(), 1);
        for (size_t row = 0; row < indices_.size(); ++row)
          solution_(row, 0).set_one();
        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);
        integer delta_numerator;
        for (size_t row = 0; row < indices_.size(); ++row)
          delta_numerator += solution_(row, 0);
        positive_definite = delta_numerator.sign() < 0;
      }
    } else if (indices_.size() - factorization_.rank() == 1 &&
               factorization_.is_positive_semidefinite()) {
      solution_.resize(indices_.size(), 1);
      factorization_.one_nullspace_vector(solution_, principal_);
      integer kernel_sum;
      for (size_t row = 0; row < indices_.size(); ++row)
        kernel_sum += solution_(row, 0);
      positive_definite = !kernel_sum.is_zero();
    }

    if (positive_definite) {
      COPOSIT_G1_DIAGNOSTICS("covered-curvature-exact-good", indices_.size());
      return true;
    }
    add_curvature_certificate(matrix.rows());
    COPOSIT_G1_DIAGNOSTICS("covered-curvature-certificate", indices_.size());
    return true;
  }

  void install_pair_curvature_certificates(const matrix_integer &matrix) {
    integer curvature;
    integer doubled_off_diagonal;
    for (size_t first = 0; first < matrix.rows(); ++first) {
      timeout_checkpoint();
      for (size_t second = first + 1; second < matrix.rows(); ++second) {
        curvature = matrix(first, first);
        curvature += matrix(second, second);
        doubled_off_diagonal = matrix(first, second);
        doubled_off_diagonal.multiply(2);
        curvature -= doubled_off_diagonal;
        if (curvature.sign() > 0)
          continue;

        support lower(matrix.rows());
        lower.set(first);
        lower.set(second);
        supports_.add_forbidden(std::move(lower));
        diagnostics_.certificate(matrix.rows() - 2);
        COPOSIT_G1_DIAGNOSTICS("pair-curvature-certificate", 2);
      }
    }
  }

  bool process_subset(const matrix_integer &matrix) {
    diagnostics_.secondary();
    const size_t dimension = indices_.size();
    principal_.resize(dimension, dimension);
    solution_.resize(dimension, 1);
    copy_principal(matrix, indices_, principal_);

    const bool singular = factorization_.factorize_inplace(principal_) == 0;
    if (singular && diagnostics_.active())
      diagnostics_.singular_support(dimension - factorization_.rank());
    if (singular)
      return process_singular_subset(matrix);
    return process_nonsingular_subset(matrix);
  }

  bool process_singular_subset(const matrix_integer &matrix) {
    factorization_.one_nullspace_vector(solution_, principal_);

    bool has_positive_entry = false;
    bool has_negative_entry = false;
    for (size_t row = 0; row < solution_.rows(); ++row) {
      has_positive_entry |= solution_(row, 0).sign() > 0;
      has_negative_entry |= solution_(row, 0).sign() < 0;
    }
    assert(has_positive_entry || has_negative_entry);
    if (!has_positive_entry) {
      solution_.negate();
      has_negative_entry = false;
    }

    if (!has_negative_entry) {
      if (classification_ != nullptr)
        classification_->is_strictly_copositive = false;
      else if (mode_ == copositivity_mode::strictly_copositive)
        return false;
    }

    const bool curvature_certificate =
        !singular_reduced_hessian_is_positive_definite();
    if (curvature_certificate)
      add_curvature_certificate(matrix.rows());

    calculate_product(matrix, solution_, 0, product_);
    if (has_negative_entry) {
      size_t positive_products = 0;
      size_t negative_products = 0;
      for (const integer &value : product_) {
        positive_products += value.sign() > 0;
        negative_products += value.sign() < 0;
      }
      if (negative_products > positive_products) {
        solution_.negate();
        for (integer &value : product_)
          value.negate();
      }
    }
    return add_dickinson_certificate(matrix, curvature_certificate);
  }

  bool process_nonsingular_subset(const matrix_integer &matrix) {
    const size_t dimension = indices_.size();
    for (auto &endpoint : checked_endpoint_indices_)
      endpoint.clear();
    for (size_t row = 0; row < dimension; ++row)
      solution_(row, 0).set_one();

    integer denominator;
    factorization_.solve_inplace(solution_, denominator, principal_);
    assert(denominator.sign() > 0);
    if (all_nonpositive(solution_, 0))
      return false;

    const bool curvature_certificate =
        !nonsingular_reduced_hessian_is_positive_definite();
    if (curvature_certificate)
      add_curvature_certificate(matrix.rows());

    calculate_nonsingular_product(matrix, solution_, 0, denominator, product_);
    current_score_ = score(solution_, 0, product_);
    if (!curvature_certificate)
      try_endpoint_curvature(matrix, endpoint_stage::traditional);

    if (dimension > 1 && current_score_.width + 1 < matrix.rows()) {
      prepare_directions(denominator);

      bool first_pass = true;
      bool pass_improved = false;
      do {
        pass_improved = false;
        ray_shortlist_.clear();
        for (size_t direction = 0; direction < dimension; ++direction) {
          if (first_pass)
            calculate_nonsingular_product(matrix, directions_, direction,
                                          denominator, direction_products_,
                                          direction);
          bool improved = false;
          if (!optimize_direction(direction, improved, true))
            return false;
          pass_improved |= improved;
          if (current_score_.width + 1 == matrix.rows())
            break;
        }
        first_pass = false;
      } while (pass_improved && current_score_.width + 1 < matrix.rows());

      if (!curvature_certificate)
        try_endpoint_curvature(matrix, endpoint_stage::halfspace);
      if (current_score_.width + 1 < matrix.rows()) {
        if (!try_combined_rays())
          return false;
        if (!curvature_certificate)
          try_endpoint_curvature(matrix, endpoint_stage::rays);
      }
    }

    return add_dickinson_certificate(matrix, curvature_certificate);
  }

  bool nonsingular_reduced_hessian_is_positive_definite() const {
    if (factorization_.is_positive_definite())
      return true;
    if (factorization_.negative_inertia() != 1)
      return false;

    integer delta_numerator;
    for (size_t row = 0; row < solution_.rows(); ++row)
      delta_numerator += solution_(row, 0);
    return delta_numerator.sign() < 0;
  }

  bool singular_reduced_hessian_is_positive_definite() const {
    if (solution_.rows() - factorization_.rank() != 1 ||
        !factorization_.is_positive_semidefinite())
      return false;

    integer kernel_sum;
    for (size_t row = 0; row < solution_.rows(); ++row)
      kernel_sum += solution_(row, 0);
    return !kernel_sum.is_zero();
  }

  void prepare_directions(const integer &denominator) {
    const size_t dimension = indices_.size();
    directions_.resize(dimension, dimension);
    for (size_t row = 0; row < dimension; ++row) {
      for (size_t column = 0; column < dimension; ++column) {
        if (row == column)
          directions_(row, column).set_one();
        else
          directions_(row, column).set_zero();
      }
    }

    integer direction_denominator;
    factorization_.solve_inplace(directions_, direction_denominator,
                                 principal_);
    assert(direction_denominator.compare(denominator) == 0);
    direction_products_.resize(product_.size(), dimension);
  }

  bool try_endpoint_curvature(const matrix_integer &matrix,
                              endpoint_stage stage) {
    endpoint_indices_.clear();
    for (size_t index = 0; index < product_.size(); ++index)
      if (product_[index].sign() >= 0)
        endpoint_indices_.push_back(index);

    const size_t stage_index = static_cast<size_t>(stage);
    // Pairs were classified exactly before traversal, so rechecking a two-index
    // endpoint can only duplicate that closure.
    if (endpoint_indices_.size() < 3 ||
        endpoint_indices_.size() == matrix.rows() ||
        endpoint_indices_ == indices_)
      return false;
    for (size_t earlier = 0; earlier < stage_index; ++earlier)
      if (endpoint_indices_ == checked_endpoint_indices_[earlier])
        return false;
    checked_endpoint_indices_[stage_index] = endpoint_indices_;

    floating_filter_.prepare(matrix);
    if (floating_filter_.looks_reduced_hessian_positive_definite(
            endpoint_indices_)) {
      COPOSIT_G1_DIAGNOSTICS("endpoint-curvature-screened-good",
                             endpoint_indices_.size());
      return false;
    }

    if (endpoint_reduced_hessian_is_positive_definite(matrix,
                                                      endpoint_indices_))
      return false;

    support lower(matrix.rows());
    for (const size_t index : endpoint_indices_)
      lower.set(index);
    supports_.add_forbidden(std::move(lower));
    diagnostics_.certificate(matrix.rows() - endpoint_indices_.size());
    if (stage == endpoint_stage::traditional)
      COPOSIT_G1_DIAGNOSTICS("endpoint-traditional-curvature",
                             endpoint_indices_.size());
    else if (stage == endpoint_stage::halfspace)
      COPOSIT_G1_DIAGNOSTICS("endpoint-halfspace-curvature",
                             endpoint_indices_.size());
    else
      COPOSIT_G1_DIAGNOSTICS("endpoint-rays-curvature",
                             endpoint_indices_.size());
    return true;
  }

  bool endpoint_reduced_hessian_is_positive_definite(
      const matrix_integer &matrix, const std::vector<size_t> &endpoint) {
    if (endpoint.size() < 2)
      return true;

    const size_t order = endpoint.size() - 1;
    const size_t anchor = endpoint.back();
    endpoint_principal_.resize(order, order);
    for (size_t row = 0; row < order; ++row) {
      timeout_checkpoint();
      for (size_t column = 0; column <= row; ++column) {
        endpoint_principal_(row, column) =
            matrix(endpoint[row], endpoint[column]);
        endpoint_principal_(row, column) -= matrix(endpoint[row], anchor);
        endpoint_principal_(row, column) -= matrix(anchor, endpoint[column]);
        endpoint_principal_(row, column) += matrix(anchor, anchor);
      }
    }
    endpoint_factorization_.factorize_inplace(endpoint_principal_);
    return endpoint_factorization_.is_positive_definite();
  }

  bool search_direction(size_t direction, bool remember_ray) {
    best_score_ = current_score_;
    best_numerator_.set_zero();
    best_denominator_.set_one();
    ray_best_initialized_ = false;
    ray_best_score_ = {};
    ray_best_numerator_.set_zero();
    ray_best_denominator_.set_one();
    ray_best_gains_ = 0;
    ray_best_losses_ = 0;
    negative_witness_found_ = false;

    find_breakpoints(direction);
    if (breakpoint_events_.empty())
      return true;
    sweep_direction(direction);
    if (negative_witness_found_)
      return false;
    if (remember_ray && ray_best_initialized_ && ray_best_gains_ > 0)
      retain_ray_candidate(direction);
    return true;
  }

  bool optimize_direction(size_t direction, bool &improved, bool remember_ray) {
    improved = false;
    if (!search_direction(direction, remember_ray))
      return false;
    if (best_numerator_.is_zero())
      return true;

    apply_candidate(direction, best_numerator_, best_denominator_);
    current_score_ = best_score_;
    improved = true;
    COPOSIT_G1_DIAGNOSTICS("halfspace-improvement", indices_.size());
    return true;
  }

  void find_breakpoints(size_t direction) {
    breakpoint_events_.clear();
    for (size_t row = 0; row < solution_.rows(); ++row)
      add_positive_breakpoint(solution_(row, 0), directions_(row, direction),
                              true);
    for (size_t row = 0; row < product_.size(); ++row)
      add_positive_breakpoint(product_[row],
                              direction_products_(row, direction), false);

    std::sort(breakpoint_events_.begin(), breakpoint_events_.end(),
              [](const auto &left, const auto &right) {
                return ratio_less(left.root, right.root);
              });
  }

  void add_positive_breakpoint(integer::const_reference base,
                               integer::const_reference direction,
                               bool solution_entry) {
    if (base.is_zero() || direction.is_zero() ||
        base.sign() == direction.sign())
      return;
    breakpoint_event event;
    event.root.numerator.set_abs(base);
    event.root.denominator.set_abs(direction);
    event.solution_entry = solution_entry;
    event.direction_sign = direction.sign();
    breakpoint_events_.push_back(std::move(event));
  }

  void sweep_direction(size_t direction) {
    size_t interval_lower_size = 0;
    size_t interval_positive_size = 0;
    for (size_t row = 0; row < solution_.rows(); ++row) {
      const int base_sign = solution_(row, 0).sign();
      const int direction_sign = directions_(row, direction).sign();
      interval_lower_size += base_sign != 0 || direction_sign != 0;
      interval_positive_size +=
          base_sign > 0 || (base_sign == 0 && direction_sign > 0);
    }

    size_t interval_upper_size = 0;
    size_t interval_gain_size = 0;
    size_t interval_loss_size = 0;
    for (size_t row = 0; row < product_.size(); ++row) {
      const int base_sign = product_[row].sign();
      const int direction_sign = direction_products_(row, direction).sign();
      interval_upper_size +=
          base_sign > 0 || (base_sign == 0 && direction_sign >= 0);
      interval_loss_size += base_sign == 0 && direction_sign < 0;
    }

    positive_ratio sample;
    sample.numerator = breakpoint_events_.front().root.numerator;
    sample.denominator = breakpoint_events_.front().root.denominator;
    sample.denominator.multiply(2);
    consider_signature(interval_lower_size, interval_upper_size,
                       interval_positive_size, interval_gain_size,
                       interval_loss_size, sample);

    size_t group_begin = 0;
    while (group_begin < breakpoint_events_.size() &&
           !negative_witness_found_) {
      size_t group_end = group_begin + 1;
      while (group_end < breakpoint_events_.size() &&
             ratio_equal(breakpoint_events_[group_begin].root,
                         breakpoint_events_[group_end].root))
        ++group_end;

      size_t root_lower_size = interval_lower_size;
      size_t root_upper_size = interval_upper_size;
      size_t root_positive_size = interval_positive_size;
      size_t solution_event_count = 0;
      size_t positive_solution_event_count = 0;
      size_t positive_product_event_count = 0;
      size_t negative_product_event_count = 0;
      for (size_t index = group_begin; index < group_end; ++index) {
        const breakpoint_event &event = breakpoint_events_[index];
        if (event.solution_entry) {
          --root_lower_size;
          ++solution_event_count;
          if (event.direction_sign < 0)
            --root_positive_size;
          else
            ++positive_solution_event_count;
        } else if (event.direction_sign > 0) {
          ++root_upper_size;
          ++positive_product_event_count;
        } else {
          ++negative_product_event_count;
        }
      }

      const size_t root_gain_size =
          interval_gain_size + positive_product_event_count;
      const size_t root_loss_size = interval_loss_size;
      consider_signature(root_lower_size, root_upper_size, root_positive_size,
                         root_gain_size, root_loss_size,
                         breakpoint_events_[group_begin].root);
      if (negative_witness_found_)
        return;
      interval_lower_size = root_lower_size + solution_event_count;
      interval_upper_size = root_upper_size - negative_product_event_count;
      interval_positive_size =
          root_positive_size + positive_solution_event_count;
      interval_gain_size = root_gain_size;
      interval_loss_size = root_loss_size + negative_product_event_count;

      if (group_end < breakpoint_events_.size())
        midpoint(sample, breakpoint_events_[group_begin].root,
                 breakpoint_events_[group_end].root);
      else {
        sample.numerator = breakpoint_events_[group_begin].root.numerator;
        sample.numerator += breakpoint_events_[group_begin].root.denominator;
        sample.denominator = breakpoint_events_[group_begin].root.denominator;
      }
      consider_signature(interval_lower_size, interval_upper_size,
                         interval_positive_size, interval_gain_size,
                         interval_loss_size, sample);
      group_begin = group_end;
    }
  }

  static void midpoint(positive_ratio &result, const positive_ratio &left,
                       const positive_ratio &right) {
    integer second_term;
    result.numerator.set_product(left.numerator, right.denominator);
    second_term.set_product(right.numerator, left.denominator);
    result.numerator += second_term;
    result.denominator.set_product(left.denominator, right.denominator);
    result.denominator.multiply(2);
  }

  void consider_signature(size_t lower_size, size_t upper_size,
                          size_t positive_size, size_t gains, size_t losses,
                          const positive_ratio &candidate) {
    if (positive_size == 0) {
      negative_witness_found_ = true;
      return;
    }
    assert(upper_size >= lower_size);
    const coverage_score candidate_score{upper_size - lower_size, upper_size};
    if (better_ray_candidate(candidate_score, gains, losses,
                             ray_best_initialized_, ray_best_score_,
                             ray_best_gains_, ray_best_losses_)) {
      ray_best_score_ = candidate_score;
      ray_best_numerator_ = candidate.numerator;
      ray_best_denominator_ = candidate.denominator;
      ray_best_gains_ = gains;
      ray_best_losses_ = losses;
      ray_best_initialized_ = true;
    }
    if (better_score(candidate_score, best_score_)) {
      best_score_ = candidate_score;
      best_numerator_ = candidate.numerator;
      best_denominator_ = candidate.denominator;
    }
  }

  void retain_ray_candidate(size_t direction) {
    ray_candidate candidate;
    candidate.step.numerator = ray_best_numerator_;
    candidate.step.denominator = ray_best_denominator_;
    candidate.score = ray_best_score_;
    candidate.direction = direction;
    candidate.gains = ray_best_gains_;
    candidate.losses = ray_best_losses_;

    const auto position =
        std::find_if(ray_shortlist_.begin(), ray_shortlist_.end(),
                     [&](const ray_candidate &current) {
                       return better_shortlist_candidate(candidate, current);
                     });
    if (ray_shortlist_.size() == shortlist_limit_ &&
        position == ray_shortlist_.end())
      return;
    ray_shortlist_.insert(position, std::move(candidate));
    if (ray_shortlist_.size() > shortlist_limit_)
      ray_shortlist_.pop_back();
  }

  void materialize_upper(size_t candidate_index) {
    const ray_candidate &candidate = ray_shortlist_[candidate_index];
    support &upper = shortlist_uppers_[candidate_index];
    upper.clear();
    size_t gains = 0;
    size_t losses = 0;
    for (size_t row = 0; row < product_.size(); ++row) {
      timeout_checkpoint();
      set_linear_combination(scratch_, product_[row],
                             direction_products_(row, candidate.direction),
                             candidate.step.numerator,
                             candidate.step.denominator);
      const bool current_upper = product_[row].sign() >= 0;
      const bool candidate_upper = scratch_.sign() >= 0;
      if (candidate_upper)
        upper.set(row);
      gains += !current_upper && candidate_upper;
      losses += current_upper && !candidate_upper;
    }
    assert(gains == candidate.gains);
    assert(losses == candidate.losses);
  }

  pair_score score_pair(size_t first, size_t second) const {
    timeout_checkpoint();
    pair_score result;
    for (size_t row = 0; row < product_.size(); ++row) {
      const bool base_upper = product_[row].sign() >= 0;
      const bool first_upper = shortlist_uppers_[first].contains(row);
      const bool second_upper = shortlist_uppers_[second].contains(row);
      result.union_gains += !base_upper && (first_upper || second_upper);
      result.common_losses += base_upper && !first_upper && !second_upper;
      result.total_gains += !base_upper && first_upper;
      result.total_gains += !base_upper && second_upper;
      result.union_losses += base_upper && (!first_upper || !second_upper);
    }
    return result;
  }

  std::array<ray_pair, 2> select_pairs() const {
    std::array<ray_pair, 2> selected;
    for (size_t first = 0; first < ray_shortlist_.size(); ++first) {
      for (size_t second = first + 1; second < ray_shortlist_.size();
           ++second) {
        ray_pair candidate{first, second, score_pair(first, second), true};
        if (better_pair(candidate, selected[0])) {
          selected[1] = selected[0];
          selected[0] = candidate;
        } else if (better_pair(candidate, selected[1])) {
          selected[1] = candidate;
        }
      }
    }
    return selected;
  }

  void build_combined_direction(const ray_pair &pair, size_t target_column) {
    const ray_candidate &first = ray_shortlist_[pair.first];
    const ray_candidate &second = ray_shortlist_[pair.second];
    integer first_coefficient;
    integer second_coefficient;
    first_coefficient.set_product(first.step.numerator,
                                  second.step.denominator);
    second_coefficient.set_product(second.step.numerator,
                                   first.step.denominator);

    integer content;
    fmpz_gcd(content.native_handle(), first_coefficient.native_handle(),
             second_coefficient.native_handle());
    if (!content.is_one()) {
      first_coefficient.divide_exact(content);
      second_coefficient.divide_exact(content);
    }

    for (size_t row = 0; row < directions_.rows(); ++row) {
      scratch_.set_product(directions_(row, first.direction),
                           first_coefficient);
      scratch_.addmul(directions_(row, second.direction), second_coefficient);
      combined_directions_(row, target_column) = scratch_;
    }
    for (size_t row = 0; row < direction_products_.rows(); ++row) {
      scratch_.set_product(direction_products_(row, first.direction),
                           first_coefficient);
      scratch_.addmul(direction_products_(row, second.direction),
                      second_coefficient);
      combined_products_(row, target_column) = scratch_;
    }
  }

  void install_combined_direction(size_t source_column) {
    for (size_t row = 0; row < directions_.rows(); ++row)
      directions_(row, 0) = combined_directions_(row, source_column);
    for (size_t row = 0; row < direction_products_.rows(); ++row)
      direction_products_(row, 0) = combined_products_(row, source_column);
  }

  bool try_combined_rays() {
    if (ray_shortlist_.size() < 2)
      return true;
    for (size_t candidate = 0; candidate < ray_shortlist_.size(); ++candidate)
      materialize_upper(candidate);

    const std::array<ray_pair, 2> pairs = select_pairs();
    if (!pairs[0].initialized)
      return true;
    const size_t ray_count = pairs[1].initialized ? 2 : 1;
    combined_directions_.resize(directions_.rows(), ray_count);
    combined_products_.resize(direction_products_.rows(), ray_count);
    for (size_t ray = 0; ray < ray_count; ++ray)
      build_combined_direction(pairs[ray], ray);

    coverage_score selected_score = current_score_;
    integer selected_numerator;
    integer selected_denominator;
    size_t selected_ray = 0;
    bool selected = false;
    for (size_t ray = 0; ray < ray_count; ++ray) {
      install_combined_direction(ray);
      COPOSIT_G1_DIAGNOSTICS("combined-ray", ray + 1);
      if (!search_direction(0, false))
        return false;
      if (!best_numerator_.is_zero() &&
          better_score(best_score_, selected_score)) {
        selected_score = best_score_;
        selected_numerator = best_numerator_;
        selected_denominator = best_denominator_;
        selected_ray = ray;
        selected = true;
      }
      if (selected_score.width + 1 == product_.size())
        break;
    }

    if (!selected)
      return true;
    install_combined_direction(selected_ray);
    apply_candidate(0, selected_numerator, selected_denominator);
    current_score_ = selected_score;
    return true;
  }

  void apply_candidate(size_t direction, const integer &numerator,
                       const integer &denominator) {
    for (size_t row = 0; row < solution_.rows(); ++row) {
      set_linear_combination(scratch_, solution_(row, 0),
                             directions_(row, direction), numerator,
                             denominator);
      solution_(row, 0) = scratch_;
    }
    for (size_t row = 0; row < product_.size(); ++row) {
      set_linear_combination(scratch_, product_[row],
                             direction_products_(row, direction), numerator,
                             denominator);
      product_[row] = scratch_;
    }
    remove_common_content();
  }

  void remove_common_content() {
    integer content;
    integer next;
    for (size_t row = 0; row < solution_.rows(); ++row) {
      fmpz_gcd(next.native_handle(), content.native_handle(),
               solution_(row, 0).native_handle());
      content = next;
      if (content.is_one())
        return;
    }
    if (content.is_zero() || content.is_one())
      return;
    for (size_t row = 0; row < solution_.rows(); ++row)
      solution_(row, 0).divide_exact(content);
    for (integer &value : product_)
      value.divide_exact(content);
  }

  static bool all_nonpositive(const matrix_integer &vectors, size_t column) {
    bool result = true;
    for (size_t row = 0; row < vectors.rows(); ++row)
      result &= vectors(row, column).sign() <= 0;
    return result;
  }

  static coverage_score score(const matrix_integer &vectors,
                              size_t vector_column,
                              const std::vector<integer> &products) {
    size_t lower_size = 0;
    for (size_t row = 0; row < vectors.rows(); ++row)
      lower_size += !vectors(row, vector_column).is_zero();
    size_t upper_size = 0;
    for (const integer &value : products)
      upper_size += value.sign() >= 0;
    assert(upper_size >= lower_size);
    return {upper_size - lower_size, upper_size};
  }

  static void set_linear_combination(integer &result,
                                     integer::const_reference base,
                                     integer::const_reference direction,
                                     integer::const_reference numerator,
                                     integer::const_reference denominator) {
    result.set_product(base, denominator);
    result.addmul(direction, numerator);
  }

  void calculate_product(const matrix_integer &matrix,
                         const matrix_integer &vectors, size_t vector_column,
                         std::vector<integer> &product) {
    for (integer &value : product)
      value.set_zero();
    for (size_t row = 0; row < matrix.rows(); ++row) {
      timeout_checkpoint();
      for (size_t local = 0; local < indices_.size(); ++local)
        product[row].addmul(matrix(row, indices_[local]),
                            vectors(local, vector_column));
    }
  }

  void calculate_nonsingular_product(const matrix_integer &matrix,
                                     const matrix_integer &vectors,
                                     size_t vector_column,
                                     const integer &denominator,
                                     std::vector<integer> &product) {
    size_t local_row = 0;
    for (size_t row = 0; row < matrix.rows(); ++row) {
      timeout_checkpoint();
      product[row].set_zero();
      if (local_row < indices_.size() && row == indices_[local_row]) {
        product[row] = denominator;
        ++local_row;
        continue;
      }
      for (size_t local = 0; local < indices_.size(); ++local)
        product[row].addmul(matrix(row, indices_[local]),
                            vectors(local, vector_column));
    }
  }

  void calculate_nonsingular_product(const matrix_integer &matrix,
                                     const matrix_integer &vectors,
                                     size_t vector_column,
                                     const integer &denominator,
                                     matrix_integer &products,
                                     size_t product_column) {
    size_t local_row = 0;
    for (size_t row = 0; row < matrix.rows(); ++row) {
      timeout_checkpoint();
      products(row, product_column).set_zero();
      if (local_row < indices_.size() && row == indices_[local_row]) {
        if (local_row == vector_column)
          products(row, product_column) = denominator;
        ++local_row;
        continue;
      }
      for (size_t local = 0; local < indices_.size(); ++local)
        products(row, product_column)
            .addmul(matrix(row, indices_[local]),
                    vectors(local, vector_column));
    }
  }

  void add_curvature_certificate(size_t matrix_dimension) {
    support lower(matrix_dimension);
    for (const size_t index : indices_)
      lower.set(index);
    supports_.add_forbidden(std::move(lower));
    diagnostics_.certificate(matrix_dimension - indices_.size());
    COPOSIT_G1_DIAGNOSTICS("curvature-certificate", indices_.size());
  }

  bool add_dickinson_certificate(const matrix_integer &matrix,
                                 bool curvature_certificate) {
    bool solution_nonnegative = true;
    integer quadratic;
    for (size_t local = 0; local < indices_.size(); ++local) {
      solution_nonnegative &= solution_(local, 0).sign() >= 0;
      quadratic.addmul(solution_(local, 0), product_[indices_[local]]);
    }
    if (solution_nonnegative && quadratic.is_zero()) {
      if (classification_ != nullptr)
        classification_->is_strictly_copositive = false;
      else if (mode_ == copositivity_mode::strictly_copositive)
        return false;
    }

    support lower(product_.size());
    size_t lower_size = 0;
    for (size_t local = 0; local < indices_.size(); ++local) {
      if (solution_(local, 0).is_zero())
        continue;
      lower.set(indices_[local]);
      ++lower_size;
    }

    support upper(product_.size());
    size_t upper_size = 0;
    for (size_t index = 0; index < product_.size(); ++index) {
      if (product_[index].sign() < 0)
        continue;
      upper.set(index);
      ++upper_size;
    }
    assert(lower.is_subset_of(upper));

    if (upper_size == product_.size() && curvature_certificate &&
        lower_size == indices_.size()) {
      COPOSIT_G1_DIAGNOSTICS("duplicate-ceiling-certificate", lower_size);
      return true;
    }

    diagnostics_.certificate(upper_size - lower_size);
    if (upper_size == product_.size()) {
      supports_.add_forbidden(std::move(lower));
      COPOSIT_G1_DIAGNOSTICS("ceiling-certificate", lower_size);
    } else {
      upper.copy_indices_to(endpoint_indices_);
      const bool check_curvature =
          !endpoint_reduced_hessian_is_positive_definite(matrix,
                                                         endpoint_indices_);
      intervals_.add(std::move(lower), std::move(upper), check_curvature);
      COPOSIT_G1_DIAGNOSTICS("bounded-certificate", lower_size);
      if (check_curvature)
        COPOSIT_G1_DIAGNOSTICS("bounded-certificate-check-curvature",
                               lower_size);
      else
        COPOSIT_G1_DIAGNOSTICS("bounded-certificate-skip-curvature",
                               lower_size);
    }
    return true;
  }

  static void copy_principal(const matrix_integer &matrix,
                             const std::vector<size_t> &indices,
                             matrix_integer &principal) {
    for (size_t row = 0; row < indices.size(); ++row) {
      timeout_checkpoint();
      for (size_t column = 0; column <= row; ++column)
        principal(row, column) = matrix(indices[row], indices[column]);
    }
  }

  fraction_free_ldlt_factorization factorization_;
  fraction_free_ldlt_factorization endpoint_factorization_;
  floating_reduced_curvature_filter floating_filter_;
  matrix_integer principal_;
  matrix_integer endpoint_principal_;
  matrix_integer solution_;
  matrix_integer directions_;
  matrix_integer direction_products_;
  matrix_integer combined_directions_;
  matrix_integer combined_products_;
  std::vector<integer> product_;
  size_t shortlist_limit_;
  std::vector<size_t> indices_;
  std::vector<size_t> endpoint_indices_;
  std::array<std::vector<size_t>, 3> checked_endpoint_indices_;
  std::vector<breakpoint_event> breakpoint_events_;
  std::vector<ray_candidate> ray_shortlist_;
  std::vector<support> shortlist_uppers_;
  coverage_score current_score_;
  coverage_score best_score_;
  coverage_score ray_best_score_;
  integer best_numerator_;
  integer best_denominator_;
  integer ray_best_numerator_;
  integer ray_best_denominator_;
  integer scratch_;
  size_t ray_best_gains_ = 0;
  size_t ray_best_losses_ = 0;
  bool ray_best_initialized_ = false;
  bool negative_witness_found_ = false;
  const copositivity_mode mode_;
  copositivity_classification *classification_ = nullptr;
  diagnostics::tracker diagnostics_;
  support_generator supports_;
  bounded_interval_index intervals_;
};

} // namespace

bool solve(const matrix_integer &matrix, copositivity_mode mode) {
  timeout_checkpoint();
  return dickinson_checker(matrix.rows(), mode).check(matrix);
}

copositivity_classification classify(const matrix_integer &matrix) {
  timeout_checkpoint();
  copositivity_classification result{true, true};
  if (!dickinson_checker(matrix.rows(), result).check(matrix))
    result = {false, false};
  return result;
}

#ifdef COPOSIT_G1_TESTING
support support_from_mask(size_t dimension, uint64_t mask) {
  support result(dimension);
  for (size_t bit = 0; bit < dimension; ++bit)
    if ((mask & (uint64_t{1} << bit)) != 0)
      result.set(bit);
  return result;
}

uint64_t mask_from_support(const support &value) {
  assert(value.dimension() <= 64);
  uint64_t result = 0;
  for (size_t bit = 0; bit < value.dimension(); ++bit)
    if (value.contains(bit))
      result |= uint64_t{1} << bit;
  return result;
}

std::vector<uint64_t> g1_generated_masks(size_t dimension,
                                         uint64_t forbidden_trigger) {
  assert(dimension <= 64);
  support_generator generator(dimension);
  std::vector<uint64_t> result;
  generator.generate([](size_t) {},
                     [&](const support &current, size_t) {
                       const uint64_t mask = mask_from_support(current);
                       result.push_back(mask);
                       if (mask == forbidden_trigger)
                         generator.add_forbidden(current);
                       return true;
                     });
  return result;
}

std::vector<uint64_t> g1_compact_masks(size_t dimension,
                                       size_t completed_cardinality,
                                       const std::vector<uint64_t> &roots) {
  assert(dimension <= 64);
  support_generator generator(dimension);
  for (const uint64_t root : roots)
    generator.add_forbidden(support_from_mask(dimension, root));
  generator.compact_for_testing(completed_cardinality);
  if (generator.all_future_forbidden_for_testing())
    return {0};

  std::vector<uint64_t> result;
  for (const support &root : generator.roots_for_testing())
    result.push_back(mask_from_support(root));
  std::sort(result.begin(), result.end());
  return result;
}

std::pair<bool, size_t> g1_interval_contains(size_t dimension, uint64_t lower,
                                             uint64_t upper, uint64_t candidate,
                                             size_t cardinality) {
  assert(dimension <= 64);
  bounded_interval_index intervals(dimension);
  intervals.add(support_from_mask(dimension, lower),
                support_from_mask(dimension, upper), false);
  intervals.start_cardinality(cardinality);
  return {intervals.coverage(support_from_mask(dimension, candidate)) !=
              bounded_coverage::none,
          intervals.size_for_testing()};
}

bool g1_overlapping_intervals_check_curvature(
    size_t dimension, uint64_t first_lower, uint64_t first_upper,
    uint64_t second_lower, uint64_t second_upper, uint64_t candidate) {
  assert(dimension <= 64);
  bounded_interval_index intervals(dimension);
  intervals.add(support_from_mask(dimension, first_lower),
                support_from_mask(dimension, first_upper), false);
  intervals.add(support_from_mask(dimension, second_lower),
                support_from_mask(dimension, second_upper), true);
  return intervals.coverage(support_from_mask(dimension, candidate)) ==
         bounded_coverage::check_curvature;
}

bool g1_check_support_for_testing(const matrix_integer &matrix,
                                  const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .check_support_for_testing(matrix, indices);
}
#endif

} // namespace coposit::model
