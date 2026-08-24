#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/improved_nbc_upward_supports.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"
#include "tiny_closed_cone_extension.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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

bool negative_orientation_has_larger_upper(size_t positive_products,
                                           size_t negative_products) noexcept {
  return negative_products > positive_products;
}

class floating_positive_semidefinite_filter {
public:
  explicit floating_positive_semidefinite_filter(size_t dimension)
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

  bool looks_positive_semidefinite(const std::vector<size_t> &indices) {
    const size_t order = indices.size();
    principal_.resize(order * order);
    diagonal_.resize(order);
    double scale = 0.0;
    for (size_t row = 0; row < order; ++row) {
      for (size_t column = 0; column <= row; ++column) {
        const double entry =
            matrix_[indices[row] * dimension_ + indices[column]];
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
      if (!std::isfinite(pivot) || pivot < -tolerance)
        return false;
      if (pivot <= tolerance) {
        diagonal_[column] = 0.0;
        for (size_t row = column + 1; row < order; ++row) {
          double entry = principal_[row * order + column];
          for (size_t previous = 0; previous < column; ++previous) {
            entry -= principal_[row * order + previous] *
                     principal_[column * order + previous] *
                     diagonal_[previous];
          }
          if (!std::isfinite(entry) || std::abs(entry) > tolerance)
            return false;
          principal_[row * order + column] = 0.0;
        }
        continue;
      }
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

#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
size_t last_optimized_certificate_count = 0;
size_t last_combined_ray_sweep_count = 0;
size_t last_combined_ray_improvement_count = 0;
size_t last_fixed_support_upper_size = 0;
size_t last_pair_curvature_exclusion_count = 0;
size_t last_support_curvature_exclusion_count = 0;
size_t last_downward_count = 0;
size_t last_high_float_rejection_count = 0;
size_t last_closed_cone_attempt_count = 0;
size_t last_closed_cone_feasible_count = 0;
size_t last_closed_cone_extension_count = 0;
size_t last_closed_cone_upper_gain = 0;
size_t last_closed_cone_exact_rejection_count = 0;
#endif

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

class dickinson_checker {
public:
  dickinson_checker(size_t dimension, copositivity_mode mode)
      : factorization_(dimension), floating_filter_(dimension),
        product_(dimension),
        shortlist_limit_(ray_shortlist_limit(dimension, dimension)),
        mode_(mode), diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
    ray_shortlist_.reserve(shortlist_limit_);
    shortlist_uppers_.reserve(shortlist_limit_);
    for (size_t index = 0; index < shortlist_limit_; ++index)
      shortlist_uppers_.emplace_back(dimension);
  }

  dickinson_checker(size_t dimension,
                    copositivity_classification &classification)
      : factorization_(dimension), floating_filter_(dimension),
        product_(dimension),
        shortlist_limit_(ray_shortlist_limit(dimension, dimension)),
        mode_(copositivity_mode::copositive), classification_(&classification),
        diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
    ray_shortlist_.reserve(shortlist_limit_);
    shortlist_uppers_.reserve(shortlist_limit_);
    for (size_t index = 0; index < shortlist_limit_; ++index)
      shortlist_uppers_.emplace_back(dimension);
  }

  bool check(const matrix_integer &matrix) {
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
    optimized_certificate_count_ = 0;
    combined_ray_sweep_count_ = 0;
    combined_ray_improvement_count_ = 0;
    pair_curvature_exclusion_count_ = 0;
    support_curvature_exclusion_count_ = 0;
    downward_count_ = 0;
    high_float_rejection_count_ = 0;
    closed_cone_attempt_count_ = 0;
    closed_cone_feasible_count_ = 0;
    closed_cone_extension_count_ = 0;
    closed_cone_upper_gain_ = 0;
    closed_cone_exact_rejection_count_ = 0;
#endif
    supports_.emplace(matrix.rows());
    install_pair_curvature_exclusions(matrix);

    size_t low = 1;
    size_t high = matrix.rows();
    while (low <= matrix.rows()) {
      if (!process_low_support(matrix, low, matrix.rows()))
        return finish(false);
      if (supports_->all_future_covered())
        return finish(true);
      if (low <= high && !process_high_support(matrix, low, high))
        return finish(false);
      if (supports_->all_future_covered())
        return finish(true);
    }
    return finish(true);
  }

#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
  bool check_support_for_testing(const matrix_integer &matrix,
                                 const std::vector<size_t> &indices) {
    support lower(matrix.rows());
    support upper(matrix.rows());
    const bool result =
        optimize_support_for_testing(matrix, indices, lower, upper);
    last_fixed_support_upper_size = 0;
    for (size_t index = 0; index < matrix.rows(); ++index)
      last_fixed_support_upper_size += upper.contains(index);
    return result;
  }

  bool optimize_support_for_testing(const matrix_integer &matrix,
                                    const std::vector<size_t> &indices,
                                    support &lower, support &upper) {
    optimized_certificate_count_ = 0;
    combined_ray_sweep_count_ = 0;
    combined_ray_improvement_count_ = 0;
    closed_cone_attempt_count_ = 0;
    closed_cone_feasible_count_ = 0;
    closed_cone_extension_count_ = 0;
    closed_cone_upper_gain_ = 0;
    closed_cone_exact_rejection_count_ = 0;
    indices_ = indices;
    captured_lower_ = &lower;
    captured_upper_ = &upper;
    const bool result = process_subset(matrix);
    captured_lower_ = nullptr;
    captured_upper_ = nullptr;
    publish_test_counters();
    return result;
  }

  bool reduced_hessian_is_positive_definite_for_testing(
      const matrix_integer &matrix, const std::vector<size_t> &indices) {
    indices_ = indices;
    principal_.resize(indices.size(), indices.size());
    solution_.resize(indices.size(), 1);
    copy_principal(matrix, indices_, principal_);
    if (factorization_.factorize_inplace(principal_) == 0) {
      factorization_.one_nullspace_vector(solution_, principal_);
      return singular_reduced_hessian_is_positive_definite();
    }
    for (size_t row = 0; row < indices.size(); ++row)
      solution_(row, 0).set_one();
    integer denominator;
    factorization_.solve_inplace(solution_, denominator, principal_);
    return nonsingular_reduced_hessian_is_positive_definite();
  }
#endif

private:
  enum class pruning_direction { upward, downward, both };

  static void append_indices(std::ostringstream &event,
                             const std::vector<size_t> &indices) {
    event << '[';
    for (size_t position = 0; position < indices.size(); ++position) {
      if (position != 0)
        event << ',';
      event << indices[position] + 1;
    }
    event << ']';
  }

  static void append_support(std::ostringstream &event,
                             const support &indices) {
    event << '[';
    bool first = true;
    for (size_t index = 0; index < indices.dimension(); ++index) {
      if (!indices.contains(index))
        continue;
      if (!first)
        event << ',';
      event << index + 1;
      first = false;
    }
    event << ']';
  }

  void record_interval_certificate(const support &lower, const support &upper) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=improved_nbc_g2 n=" << product_.size()
          << " frontier=" << certificate_frontier_ << " kind=dickinson source=";
    append_indices(event, indices_);
    event << " coverage=interval lower=";
    append_support(event, lower);
    event << " upper=";
    append_support(event, upper);
    event << " exclude_empty=no floating_checked=no exact_checked=yes";
    diagnostics::record_history_event("certificate", event.str());
  }

  void record_closure_certificate(std::string_view kind,
                                  std::string_view coverage,
                                  std::string_view frontier,
                                  const std::vector<size_t> &source) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=improved_nbc_g2 n=" << product_.size()
          << " frontier=" << frontier << " kind=" << kind << " source=";
    append_indices(event, source);
    event << " coverage=" << coverage;
    if (coverage == "upward") {
      event << " lower=";
      append_indices(event, source);
      event << " upper=all exclude_empty=no";
    } else {
      event << " lower=[] upper=";
      append_indices(event, source);
      event << " exclude_empty=yes";
    }
    event << " floating_checked=" << (frontier == "high" ? "yes" : "no")
          << " exact_checked=yes";
    diagnostics::record_history_event("certificate", event.str());
  }

  void record_high_support_without_certificate(bool exact_checked) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=improved_nbc_g2 n=" << product_.size()
          << " frontier=high source=";
    append_indices(event, indices_);
    event << " floating_checked=yes exact_checked="
          << (exact_checked ? "yes" : "no");
    diagnostics::record_history_event("visited_support", event.str());
  }

  void record_pair_curvature_certificate(size_t first, size_t second) {
    record_closure_certificate("pair_curvature", "upward", "initial",
                               {first, second});
  }

  bool process_low_support(const matrix_integer &matrix, size_t &low,
                           size_t high) {
    while (low <= high) {
      diagnostics_.stage(low);
      COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS("stage_low", low);
      supports_->start_cardinality(low);
      if (supports_->take_first(indices_))
        return process_selected_support(matrix,
                                        low == high ? pruning_direction::both
                                                    : pruning_direction::upward,
                                        true);
      ++low;
      supports_->commit_frontiers(low, matrix.rows());
      if (supports_->all_future_covered())
        return true;
    }
    return true;
  }

  bool process_high_support(const matrix_integer &matrix, size_t low,
                            size_t &high) {
    while (low <= high) {
      diagnostics_.stage(high);
      COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS("stage_high", high);
      supports_->start_cardinality(high, true);
      if (supports_->take_first(indices_, true))
        return process_selected_support(matrix, pruning_direction::downward,
                                        false);
      --high;
      supports_->commit_frontiers(low, matrix.rows());
      if (supports_->all_future_covered())
        return true;
    }
    return true;
  }

  bool process_selected_support(const matrix_integer &matrix,
                                pruning_direction direction,
                                bool from_low_frontier) {
    timeout_checkpoint();
    certificate_frontier_ = from_low_frontier ? "low" : "high";
    diagnostics_.visit_support();
    diagnostics_.secondary();
    COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS(
        from_low_frontier ? "process_low" : "process_high", indices_.size());
    return process_subset(matrix, direction, from_low_frontier);
  }

  void install_pair_curvature_exclusions(const matrix_integer &matrix) {
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

        supports_->add_pair_upward_closure(first, second);
        diagnostics_.certificate();
        record_pair_curvature_certificate(first, second);
        COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS("pair_upward", 2);
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
        ++pair_curvature_exclusion_count_;
#endif
      }
    }
  }

  bool process_subset(const matrix_integer &matrix,
                      pruning_direction direction = pruning_direction::upward,
                      bool from_low_frontier = true) {
    if (!from_low_frontier) {
      floating_filter_.prepare(matrix);
      if (!floating_filter_.looks_positive_semidefinite(indices_)) {
        record_high_support_without_certificate(false);
        COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS("high_float_reject",
                                            indices_.size());
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
        ++high_float_rejection_count_;
#endif
        return true;
      }
    }

    const size_t dimension = indices_.size();
    principal_.resize(dimension, dimension);
    solution_.resize(dimension, 1);
    copy_principal(matrix, indices_, principal_);

    const bool singular = factorization_.factorize_inplace(principal_) == 0;
    if (singular && diagnostics_.active())
      diagnostics_.singular_support(dimension - factorization_.rank());
    if (!from_low_frontier) {
      if (singular)
        return process_curvature_only_singular(direction);
      return process_curvature_only_nonsingular(direction);
    }
    if (direction == pruning_direction::both && singular &&
        add_singular_psd_downward_closure())
      return true;
    if (direction == pruning_direction::both && !singular &&
        factorization_.is_positive_definite()) {
      add_downward_closure("positive_definite");
      return true;
    }
    if (singular)
      return process_singular_subset(matrix);
    return process_nonsingular_subset(matrix);
  }

  bool process_curvature_only_nonsingular(pruning_direction direction) {
    if (factorization_.is_positive_definite()) {
      if (direction != pruning_direction::upward)
        add_downward_closure("positive_definite");
      return true;
    }
    if (factorization_.negative_inertia() != 1) {
      if (direction == pruning_direction::both)
        add_upward_closure();
      else
        record_high_support_without_certificate(true);
      return true;
    }

    for (size_t row = 0; row < solution_.rows(); ++row)
      solution_(row, 0).set_one();
    integer denominator;
    factorization_.solve_inplace(solution_, denominator, principal_);
    assert(denominator.sign() > 0);

    integer delta_numerator;
    bool all_nonpositive = true;
    for (size_t row = 0; row < solution_.rows(); ++row) {
      delta_numerator += solution_(row, 0);
      all_nonpositive &= solution_(row, 0).sign() <= 0;
    }
    if (delta_numerator.sign() >= 0) {
      if (direction == pruning_direction::both)
        add_upward_closure();
      else
        record_high_support_without_certificate(true);
      return true;
    }
    if (all_nonpositive) {
      record_high_support_without_certificate(true);
      return false;
    }

    record_high_support_without_certificate(true);
    return true;
  }

  bool process_curvature_only_singular(pruning_direction direction) {
    if (direction != pruning_direction::upward &&
        add_singular_psd_downward_closure())
      return true;

    if (solution_.rows() - factorization_.rank() != 1 ||
        !factorization_.is_positive_semidefinite()) {
      if (direction == pruning_direction::both)
        add_upward_closure();
      else
        record_high_support_without_certificate(true);
      return true;
    }

    factorization_.one_nullspace_vector(solution_, principal_);
    integer kernel_sum;
    bool all_nonnegative = true;
    bool all_nonpositive = true;
    for (size_t row = 0; row < solution_.rows(); ++row) {
      kernel_sum += solution_(row, 0);
      all_nonnegative &= solution_(row, 0).sign() >= 0;
      all_nonpositive &= solution_(row, 0).sign() <= 0;
    }
    if (kernel_sum.is_zero()) {
      if (direction == pruning_direction::both)
        add_upward_closure();
      else
        record_high_support_without_certificate(true);
      return true;
    }

    if (all_nonnegative || all_nonpositive) {
      if (classification_ != nullptr)
        classification_->is_strictly_copositive = false;
      else if (mode_ == copositivity_mode::strictly_copositive) {
        record_high_support_without_certificate(true);
        return false;
      }
    }
    record_high_support_without_certificate(true);
    return true;
  }

  bool add_singular_psd_downward_closure() {
    if (!factorization_.is_positive_semidefinite())
      return false;
    for (size_t row = 0; row < solution_.rows(); ++row)
      solution_(row, 0).set_one();
    integer denominator;
    if (!factorization_.solve_consistent_inplace(solution_, denominator,
                                                 principal_))
      return false;
    add_downward_closure("singular_psd_consistent");
    return true;
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

    if (!singular_reduced_hessian_is_positive_definite())
      return add_curvature_exclusion();

    calculate_product(matrix, solution_, 0, product_);
    if (has_negative_entry) {
      size_t positive_products = 0;
      size_t negative_products = 0;
      for (const integer &value : product_) {
        positive_products += value.sign() > 0;
        negative_products += value.sign() < 0;
      }
      if (negative_orientation_has_larger_upper(positive_products,
                                                negative_products)) {
        solution_.negate();
        for (integer &value : product_)
          value.negate();
      }
    }
    return add_certificate();
  }

  bool process_nonsingular_subset(const matrix_integer &matrix) {
    const size_t dimension = indices_.size();
    for (size_t row = 0; row < dimension; ++row)
      solution_(row, 0).set_one();

    integer denominator;
    factorization_.solve_inplace(solution_, denominator, principal_);
    assert(denominator.sign() > 0);
    if (all_nonpositive(solution_, 0))
      return false;
    if (!nonsingular_reduced_hessian_is_positive_definite())
      return add_curvature_exclusion();

    calculate_nonsingular_product(matrix, solution_, 0, denominator, product_);
    current_score_ = score(solution_, 0, product_);
    if (dimension > 1 && current_score_.width + 1 < matrix.rows()) {
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

      direction_products_.resize(matrix.rows(), dimension);

      bool first_pass = true;
      bool pass_improved = false;
      do {
        pass_improved = false;
        ray_shortlist_.clear();
        for (size_t direction = 0; direction < dimension; ++direction) {
          if (first_pass)
            calculate_nonsingular_product(matrix, directions_, direction,
                                          direction_denominator,
                                          direction_products_, direction);
          bool improved = false;
          if (!optimize_direction(direction, improved, true))
            return false;
          pass_improved |= improved;
          if (current_score_.width + 1 == matrix.rows())
            break;
        }
        first_pass = false;
        if (!pass_improved && current_score_.width + 1 < matrix.rows() &&
            !try_combined_rays())
          return false;
      } while (pass_improved && current_score_.width + 1 < matrix.rows());

      if (current_score_.upper_size < matrix.rows() &&
          !try_closed_cone_extension())
        return false;
    }

    return add_certificate();
  }

  enum class exact_closed_cone_result { unchanged, improved, terminal_false };

  bool try_closed_cone_extension() {
    timeout_checkpoint();
    const size_t support_dimension = indices_.size();
    const size_t outside_dimension = product_.size() - support_dimension;
    if (outside_dimension == 0 ||
        !closed_cone_problem_fits(support_dimension, outside_dimension))
      return true;

    closed_cone_outside_rows_.clear();
    closed_cone_outside_rows_.reserve(outside_dimension);
    closed_cone_scaled_rows_.assign(
        outside_dimension, std::vector<double>(support_dimension, 0.0));

    size_t outside = 0;
    size_t local_row = 0;
    for (size_t row = 0; row < product_.size(); ++row) {
      if (local_row < indices_.size() && row == indices_[local_row]) {
        ++local_row;
        continue;
      }
      if ((outside & 63U) == 0)
        timeout_checkpoint();
      closed_cone_outside_rows_.push_back(row);
      slong maximum_exponent = std::numeric_limits<slong>::min();
      for (size_t column = 0; column < support_dimension; ++column) {
        if (direction_products_(row, column).is_zero())
          continue;
        slong exponent = 0;
        static_cast<void>(
            direction_products_(row, column).to_dbl_2exp(exponent));
        maximum_exponent = std::max(maximum_exponent, exponent);
      }
      if (maximum_exponent != std::numeric_limits<slong>::min()) {
        for (size_t column = 0; column < support_dimension; ++column) {
          if (direction_products_(row, column).is_zero())
            continue;
          slong exponent = 0;
          const double mantissa =
              direction_products_(row, column).to_dbl_2exp(exponent);
          const slong difference = exponent - maximum_exponent;
          closed_cone_scaled_rows_[outside][column] =
              difference < -1074
                  ? 0.0
                  : std::scalbn(mantissa, static_cast<int>(difference));
        }
      }
      ++outside;
    }

    while (current_score_.upper_size < product_.size()) {
      closed_cone_required_rows_.clear();
      for (size_t row = 0; row < outside_dimension; ++row)
        if (product_[closed_cone_outside_rows_[row]].sign() >= 0)
          closed_cone_required_rows_.push_back(row);

      bool enlarged = false;
      const size_t preserved_count = closed_cone_required_rows_.size();
      for (size_t target = 0; target < outside_dimension; ++target) {
        timeout_checkpoint();
        if (product_[closed_cone_outside_rows_[target]].sign() >= 0)
          continue;
        closed_cone_required_rows_.resize(preserved_count);
        closed_cone_required_rows_.push_back(target);
        detail::tiny_closed_cone_extension feasibility(
            closed_cone_scaled_rows_, closed_cone_required_rows_);
        const detail::closed_cone_extension_result proposal =
            feasibility.solve();
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
        ++closed_cone_attempt_count_;
#endif
        if (!proposal.feasible)
          continue;
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
        ++closed_cone_feasible_count_;
#endif
        const size_t previous_upper = current_score_.upper_size;
        const exact_closed_cone_result result = consider_closed_cone_point(
            proposal.point, closed_cone_required_rows_);
        if (result == exact_closed_cone_result::terminal_false)
          return false;
        if (result == exact_closed_cone_result::improved) {
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
          ++closed_cone_extension_count_;
          closed_cone_upper_gain_ += current_score_.upper_size - previous_upper;
#endif
          COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS("closed_cone_extension",
                                              indices_.size());
          enlarged = true;
          break;
        }
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
        ++closed_cone_exact_rejection_count_;
#endif
      }
      if (!enlarged)
        break;
    }
    return true;
  }

  exact_closed_cone_result
  consider_closed_cone_point(const std::vector<double> &point,
                             const std::vector<size_t> &required_outside_rows) {
    closed_cone_coefficients_.resize(point.size());
    closed_cone_solution_.resize(point.size(), 1);
    closed_cone_product_.resize(product_.size());
    coverage_score selected_score = current_score_;
    bool selected = false;
    const double maximum = *std::max_element(point.begin(), point.end());
    if (!(maximum > 0.0) || !std::isfinite(maximum))
      return exact_closed_cone_result::unchanged;

    for (const uint64_t scale :
         {uint64_t{1000000}, uint64_t{1000000000}, uint64_t{1000000000000},
          uint64_t{1000000000000000}}) {
      bool valid = true;
      bool nonzero = false;
      for (size_t column = 0; column < point.size(); ++column) {
        const double value = point[column] / maximum;
        if (!(value >= 0.0) || !std::isfinite(value)) {
          valid = false;
          break;
        }
        const uint64_t coefficient = static_cast<uint64_t>(
            std::llround(value * static_cast<double>(scale)));
        nonzero |= coefficient != 0;
        fmpz_set_ui(closed_cone_coefficients_[column].native_handle(),
                    static_cast<ulong>(coefficient));
      }
      if (!valid || !nonzero)
        continue;

      multiply_directions(directions_, direction_products_,
                          closed_cone_coefficients_, closed_cone_solution_,
                          closed_cone_product_);
      bool preserves_required_rows = true;
      for (const size_t row : required_outside_rows) {
        if (closed_cone_product_[closed_cone_outside_rows_[row]].sign() < 0) {
          preserves_required_rows = false;
          break;
        }
      }
      if (!preserves_required_rows)
        continue;

      if (all_nonpositive(closed_cone_solution_, 0)) {
        integer quadratic;
        for (size_t local = 0; local < indices_.size(); ++local)
          quadratic.addmul(closed_cone_solution_(local, 0),
                           closed_cone_product_[indices_[local]]);
        assert(quadratic.sign() <= 0);
        if (quadratic.sign() < 0)
          return exact_closed_cone_result::terminal_false;
        if (classification_ != nullptr)
          classification_->is_strictly_copositive = false;
        else if (mode_ == copositivity_mode::strictly_copositive)
          return exact_closed_cone_result::terminal_false;
        continue;
      }

      const coverage_score candidate_score =
          score(closed_cone_solution_, 0, closed_cone_product_);
      if (!better_score(candidate_score, selected_score))
        continue;
      selected_score = candidate_score;
      selected_solution_ = closed_cone_solution_;
      selected_product_ = closed_cone_product_;
      selected = true;
    }

    if (!selected)
      return exact_closed_cone_result::unchanged;
    solution_ = selected_solution_;
    product_ = selected_product_;
    current_score_ = selected_score;
    remove_common_content();
    return exact_closed_cone_result::improved;
  }

  static void multiply_directions(const matrix_integer &directions,
                                  const matrix_integer &products,
                                  const std::vector<integer> &coefficients,
                                  matrix_integer &solution,
                                  std::vector<integer> &product) {
    for (size_t row = 0; row < directions.rows(); ++row) {
      solution(row, 0).set_zero();
      for (size_t column = 0; column < directions.cols(); ++column)
        solution(row, 0).addmul(directions(row, column), coefficients[column]);
    }
    for (size_t row = 0; row < products.rows(); ++row) {
      product[row].set_zero();
      for (size_t column = 0; column < products.cols(); ++column)
        product[row].addmul(products(row, column), coefficients[column]);
    }
  }

  static bool closed_cone_problem_fits(size_t support_dimension,
                                       size_t outside_dimension) noexcept {
    constexpr size_t maximum_scaled_entries = size_t{256} * 1024;
    return support_dimension == 0 ||
           outside_dimension <= maximum_scaled_entries / support_dimension;
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
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
    ++optimized_certificate_count_;
#endif
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
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
      ++combined_ray_sweep_count_;
#endif
      COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS("combined_ray", ray + 1);
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
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
    ++optimized_certificate_count_;
    ++combined_ray_improvement_count_;
#endif
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

  bool add_certificate() {
    support lower(product_.size());
    support upper(product_.size());
    size_t lower_size = 0;
    size_t upper_size = 0;
    for (size_t local = 0; local < indices_.size(); ++local) {
      if (!solution_(local, 0).is_zero()) {
        lower.set(indices_[local]);
        ++lower_size;
      }
    }
    for (size_t row = 0; row < product_.size(); ++row) {
      if (product_[row].sign() >= 0) {
        upper.set(row);
        ++upper_size;
      }
    }

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

#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
    if (captured_lower_ != nullptr) {
      *captured_lower_ = lower;
      *captured_upper_ = upper;
      return true;
    }
#endif
    supports_->add_interval(lower, upper);
    if (diagnostics_.active())
      diagnostics_.certificate(upper_size - lower_size, upper_size);
    record_interval_certificate(lower, upper);
    COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS("dickinson", indices_.size());
    return true;
  }

  bool add_curvature_exclusion() {
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
    ++support_curvature_exclusion_count_;
    if (captured_lower_ != nullptr) {
      support lower(product_.size());
      support ceiling(product_.size());
      for (const size_t index : indices_)
        lower.set(index);
      ceiling.set_all();
      *captured_lower_ = lower;
      *captured_upper_ = ceiling;
      return true;
    }
#endif
    add_upward_closure();
    return true;
  }

  void add_upward_closure() {
    support lower(product_.size());
    support ceiling(product_.size());
    for (const size_t index : indices_)
      lower.set(index);
    ceiling.set_all();
    supports_->add_interval(lower, ceiling);
    if (diagnostics_.active())
      diagnostics_.certificate(product_.size() - indices_.size(),
                               product_.size());
    record_closure_certificate("support_curvature", "upward",
                               certificate_frontier_, indices_);
    COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS("support_upward", indices_.size());
  }

  void add_downward_closure(std::string_view kind) {
    support floor(product_.size());
    support upper(product_.size());
    for (const size_t index : indices_)
      upper.set(index);
    supports_->add_interval(floor, upper);
    diagnostics_.certificate();
    record_closure_certificate(kind, "downward", certificate_frontier_,
                               indices_);
    COPOSIT_IMPROVED_NBC_G2_DIAGNOSTICS("downward", indices_.size());
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
    ++downward_count_;
#endif
  }

  bool finish(bool result) {
    diagnostics_.finish();
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
    publish_test_counters();
#endif
    return result;
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

#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
  void publish_test_counters() const noexcept {
    last_optimized_certificate_count = optimized_certificate_count_;
    last_combined_ray_sweep_count = combined_ray_sweep_count_;
    last_combined_ray_improvement_count = combined_ray_improvement_count_;
    last_pair_curvature_exclusion_count = pair_curvature_exclusion_count_;
    last_support_curvature_exclusion_count = support_curvature_exclusion_count_;
    last_downward_count = downward_count_;
    last_high_float_rejection_count = high_float_rejection_count_;
    last_closed_cone_attempt_count = closed_cone_attempt_count_;
    last_closed_cone_feasible_count = closed_cone_feasible_count_;
    last_closed_cone_extension_count = closed_cone_extension_count_;
    last_closed_cone_upper_gain = closed_cone_upper_gain_;
    last_closed_cone_exact_rejection_count = closed_cone_exact_rejection_count_;
  }
#endif

  fraction_free_ldlt_factorization factorization_;
  floating_positive_semidefinite_filter floating_filter_;
  matrix_integer principal_;
  matrix_integer solution_;
  matrix_integer directions_;
  matrix_integer direction_products_;
  matrix_integer combined_directions_;
  matrix_integer combined_products_;
  matrix_integer closed_cone_solution_;
  matrix_integer selected_solution_;
  std::vector<integer> product_;
  std::vector<integer> closed_cone_product_;
  std::vector<integer> selected_product_;
  std::vector<integer> closed_cone_coefficients_;
  std::vector<size_t> closed_cone_outside_rows_;
  std::vector<size_t> closed_cone_required_rows_;
  std::vector<std::vector<double>> closed_cone_scaled_rows_;
  size_t shortlist_limit_;
  std::vector<size_t> indices_;
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
  std::string_view certificate_frontier_ = "direct";
  const copositivity_mode mode_;
  copositivity_classification *classification_ = nullptr;
  diagnostics::tracker diagnostics_;
  std::optional<improved_nbc_upward_supports> supports_;
#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
  size_t optimized_certificate_count_ = 0;
  size_t combined_ray_sweep_count_ = 0;
  size_t combined_ray_improvement_count_ = 0;
  size_t pair_curvature_exclusion_count_ = 0;
  size_t support_curvature_exclusion_count_ = 0;
  size_t downward_count_ = 0;
  size_t high_float_rejection_count_ = 0;
  size_t closed_cone_attempt_count_ = 0;
  size_t closed_cone_feasible_count_ = 0;
  size_t closed_cone_extension_count_ = 0;
  size_t closed_cone_upper_gain_ = 0;
  size_t closed_cone_exact_rejection_count_ = 0;
  support *captured_lower_ = nullptr;
  support *captured_upper_ = nullptr;
#endif
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

#ifdef COPOSIT_IMPROVED_NBC_G2_TESTING
bool improved_nbc_g2_prefers_negative_singular_orientation_for_testing(
    size_t positive_products, size_t negative_products) noexcept {
  return negative_orientation_has_larger_upper(positive_products,
                                               negative_products);
}

size_t improved_nbc_g2_optimized_certificate_count_for_testing() noexcept {
  return last_optimized_certificate_count;
}

size_t improved_nbc_g2_combined_ray_sweep_count_for_testing() noexcept {
  return last_combined_ray_sweep_count;
}

size_t improved_nbc_g2_combined_ray_improvement_count_for_testing() noexcept {
  return last_combined_ray_improvement_count;
}

size_t improved_nbc_g2_pair_curvature_exclusion_count_for_testing() noexcept {
  return last_pair_curvature_exclusion_count;
}

size_t
improved_nbc_g2_support_curvature_exclusion_count_for_testing() noexcept {
  return last_support_curvature_exclusion_count;
}

size_t improved_nbc_g2_pair_upward_count_for_testing() noexcept {
  return last_pair_curvature_exclusion_count;
}

size_t improved_nbc_g2_support_upward_count_for_testing() noexcept {
  return last_support_curvature_exclusion_count;
}

size_t improved_nbc_g2_downward_count_for_testing() noexcept {
  return last_downward_count;
}

size_t improved_nbc_g2_high_float_rejection_count_for_testing() noexcept {
  return last_high_float_rejection_count;
}

size_t improved_nbc_g2_closed_cone_attempt_count_for_testing() noexcept {
  return last_closed_cone_attempt_count;
}

size_t improved_nbc_g2_closed_cone_feasible_count_for_testing() noexcept {
  return last_closed_cone_feasible_count;
}

size_t improved_nbc_g2_closed_cone_extension_count_for_testing() noexcept {
  return last_closed_cone_extension_count;
}

size_t improved_nbc_g2_closed_cone_upper_gain_for_testing() noexcept {
  return last_closed_cone_upper_gain;
}

size_t
improved_nbc_g2_closed_cone_exact_rejection_count_for_testing() noexcept {
  return last_closed_cone_exact_rejection_count;
}

bool improved_nbc_g2_floating_psd_candidate_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  floating_positive_semidefinite_filter filter(matrix.rows());
  filter.prepare(matrix);
  return filter.looks_positive_semidefinite(indices);
}

bool improved_nbc_g2_reduced_hessian_is_positive_definite_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .reduced_hessian_is_positive_definite_for_testing(matrix, indices);
}

size_t improved_nbc_g2_shortlist_limit_for_testing(size_t matrix_dimension,
                                                   size_t support_dimension) {
  return ray_shortlist_limit(matrix_dimension, support_dimension);
}

bool improved_nbc_g2_prefers_ray_candidate_for_testing(
    size_t candidate_upper, size_t candidate_width, size_t candidate_gains,
    size_t candidate_losses, size_t current_upper, size_t current_width,
    size_t current_gains, size_t current_losses) {
  return better_ray_candidate(
      {candidate_width, candidate_upper}, candidate_gains, candidate_losses,
      true, {current_width, current_upper}, current_gains, current_losses);
}

bool improved_nbc_g2_check_support_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(),
                           copositivity_mode::strictly_copositive)
      .check_support_for_testing(matrix, indices);
}

bool improved_nbc_g2_certificate_for_testing(const matrix_integer &matrix,
                                             const std::vector<size_t> &indices,
                                             support &lower, support &upper) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .optimize_support_for_testing(matrix, indices, lower, upper);
}

size_t improved_nbc_g2_fixed_support_upper_size_for_testing() noexcept {
  return last_fixed_support_upper_size;
}

bool count_uncovered_support(void *opaque, const std::vector<size_t> &) {
  ++*static_cast<size_t *>(opaque);
  return true;
}

size_t improved_nbc_g2_uncovered_count(
    size_t dimension, size_t cardinality,
    const std::vector<std::pair<uint64_t, uint64_t>> &intervals) {
  improved_nbc_upward_supports diagram(dimension);
  for (const auto &[lower_mask, upper_mask] : intervals) {
    support lower(dimension);
    support upper(dimension);
    for (size_t bit = 0; bit < dimension; ++bit) {
      if ((lower_mask & (uint64_t{1} << bit)) != 0)
        lower.set(bit);
      if ((upper_mask & (uint64_t{1} << bit)) != 0)
        upper.set(bit);
    }
    diagram.add_interval(lower, upper);
  }

  diagram.commit_frontiers(1, dimension);
  size_t count = 0;
  const auto result = diagram.enumerate_cardinality(cardinality, &count,
                                                    &count_uncovered_support);
  assert(result == improved_nbc_upward_supports::enumeration_result::exhausted);
  return count;
}

size_t improved_nbc_g2_pair_exclusion_uncovered_count(size_t dimension,
                                                      size_t cardinality,
                                                      size_t first,
                                                      size_t second) {
  improved_nbc_upward_supports diagram(dimension);
  diagram.add_pair_upward_closure(first, second);
  diagram.commit_frontiers(1, dimension);
  size_t count = 0;
  const auto result = diagram.enumerate_cardinality(cardinality, &count,
                                                    &count_uncovered_support);
  assert(result == improved_nbc_upward_supports::enumeration_result::exhausted);
  return count;
}

size_t improved_nbc_g2_uncovered_count(
    size_t dimension, size_t cardinality,
    const std::vector<std::vector<size_t>> &upward,
    const std::vector<std::vector<size_t>> &downward,
    const std::vector<std::vector<size_t>> &exact) {
  improved_nbc_upward_supports supports(dimension);
  support ceiling(dimension);
  ceiling.set_all();
  for (const auto &indices : upward) {
    support lower(dimension);
    for (const size_t index : indices)
      lower.set(index);
    supports.add_interval(lower, ceiling);
  }
  for (const auto &indices : downward) {
    support floor(dimension);
    support upper(dimension);
    for (const size_t index : indices)
      upper.set(index);
    supports.add_interval(floor, upper);
  }
  for (const auto &indices : exact) {
    support singleton(dimension);
    for (const size_t index : indices)
      singleton.set(index);
    supports.add_interval(singleton, singleton);
  }

  supports.commit_frontiers(1, dimension);
  size_t count = 0;
  const auto result = supports.enumerate_cardinality(cardinality, &count,
                                                     &count_uncovered_support);
  assert(result == improved_nbc_upward_supports::enumeration_result::exhausted);
  return count;
}

size_t improved_nbc_g2_interval_clause_size(size_t dimension,
                                            uint64_t lower_mask,
                                            uint64_t upper_mask) {
  size_t lower_size = 0;
  size_t upper_size = 0;
  for (size_t bit = 0; bit < dimension; ++bit) {
    lower_size += (lower_mask & (uint64_t{1} << bit)) != 0;
    upper_size += (upper_mask & (uint64_t{1} << bit)) != 0;
  }
  return lower_size + dimension - upper_size + (upper_size < dimension);
}
#endif

} // namespace coposit::model
