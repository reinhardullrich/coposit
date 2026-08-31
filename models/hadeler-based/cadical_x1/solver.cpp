#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <cadical.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
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
  struct extension_score {
    double pivot = 0.0;
    double tolerance = 0.0;
  };

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
        const auto &entry = matrix(row, column);
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

  bool prepare_principal_extensions(const std::vector<size_t> &indices) {
    reduced_extensions_ = false;
    extension_anchor_ = 0;
    extension_indices_ = indices;
    return factor_extension_base();
  }

  bool prepare_reduced_hessian_extensions(const std::vector<size_t> &indices) {
    if (indices.empty())
      return false;
    reduced_extensions_ = true;
    extension_anchor_ = indices.front();
    extension_indices_.assign(indices.begin() + 1, indices.end());
    return factor_extension_base();
  }

  std::optional<extension_score> score_extension(size_t candidate) {
    const size_t order = extension_indices_.size();
    extension_rhs_.resize(order);
    double scale = std::abs(extension_entry(candidate, candidate));
    for (size_t row = 0; row < order; ++row) {
      extension_rhs_[row] = extension_entry(extension_indices_[row], candidate);
      scale = std::max(scale, std::abs(extension_rhs_[row]));
    }
    scale = std::max(scale, extension_scale_);
    if (!std::isfinite(scale))
      return std::nullopt;

    double pivot = extension_entry(candidate, candidate);
    for (size_t row = 0; row < order; ++row) {
      double transformed = extension_rhs_[row];
      for (size_t previous = 0; previous < row; ++previous)
        transformed -=
            principal_[row * order + previous] * extension_rhs_[previous];
      extension_rhs_[row] = transformed;
      pivot -= transformed * transformed / diagonal_[row];
    }
    if (!std::isfinite(pivot))
      return std::nullopt;
    return extension_score{pivot, 64.0 *
                                      std::numeric_limits<double>::epsilon() *
                                      scale * static_cast<double>(order + 1)};
  }

  bool
  looks_reduced_hessian_positive_definite(const std::vector<size_t> &indices) {
    return prepare_reduced_hessian_extensions(indices);
  }

private:
  bool factor_extension_base() {
    const size_t order = extension_indices_.size();
    principal_.assign(order * order, 0.0);
    diagonal_.resize(order);
    extension_scale_ = 0.0;
    for (size_t row = 0; row < order; ++row) {
      for (size_t column = 0; column <= row; ++column) {
        const double value = extension_entry(extension_indices_[row],
                                             extension_indices_[column]);
        principal_[row * order + column] = value;
        extension_scale_ = std::max(extension_scale_, std::abs(value));
      }
    }
    if (order == 0)
      return true;
    if (!(extension_scale_ > 0.0) || !std::isfinite(extension_scale_))
      return false;

    const double tolerance = 64.0 * std::numeric_limits<double>::epsilon() *
                             extension_scale_ * static_cast<double>(order);
    for (size_t column = 0; column < order; ++column) {
      double pivot = principal_[column * order + column];
      for (size_t previous = 0; previous < column; ++previous) {
        const double multiplier = principal_[column * order + previous];
        pivot -= multiplier * multiplier * diagonal_[previous];
      }
      if (!std::isfinite(pivot) || pivot <= tolerance)
        return false;
      diagonal_[column] = pivot;
      for (size_t row = column + 1; row < order; ++row) {
        double value = principal_[row * order + column];
        for (size_t previous = 0; previous < column; ++previous)
          value -= principal_[row * order + previous] *
                   principal_[column * order + previous] * diagonal_[previous];
        value /= pivot;
        if (!std::isfinite(value))
          return false;
        principal_[row * order + column] = value;
      }
    }
    return true;
  }

  double extension_entry(size_t row, size_t column) const noexcept {
    if (!reduced_extensions_)
      return matrix_[row * dimension_ + column];
    return matrix_[row * dimension_ + column] -
           matrix_[row * dimension_ + extension_anchor_] -
           matrix_[extension_anchor_ * dimension_ + column] +
           matrix_[extension_anchor_ * dimension_ + extension_anchor_];
  }

  size_t dimension_;
  bool prepared_ = false;
  std::vector<double> matrix_;
  std::vector<double> principal_;
  std::vector<double> diagonal_;
  std::vector<double> extension_rhs_;
  std::vector<size_t> extension_indices_;
  size_t extension_anchor_ = 0;
  double extension_scale_ = 0.0;
  bool reduced_extensions_ = false;
};

#ifdef COPOSIT_CADICAL_X1_TESTING
size_t last_optimized_certificate_count = 0;
size_t last_combined_ray_sweep_count = 0;
size_t last_combined_ray_improvement_count = 0;
size_t last_fixed_support_upper_size = 0;
size_t last_pair_curvature_exclusion_count = 0;
size_t last_support_curvature_exclusion_count = 0;
size_t last_downward_count = 0;
#endif

class timeout_terminator final : public CaDiCaL::Terminator {
public:
  bool terminate() override { return timeout_pending(); }
};

class interval_sat {
public:
  explicit interval_sat(const support_context &context)
      : context_(context), dimension_(context.dimension()) {
    if (dimension_ > static_cast<size_t>(std::numeric_limits<int>::max()))
      throw std::overflow_error(
          "SAT variable count exceeds CaDiCaL's integer literal range");
    if (!solver_.configure("sat"))
      throw std::runtime_error(
          "CaDiCaL lacks its satisfiable-instance configuration");
    if (!solver_.set("ilb", 2))
      throw std::runtime_error("CaDiCaL lacks incremental lazy backtracking");
    if (!solver_.set("lucky", 0))
      throw std::runtime_error("CaDiCaL lacks configurable lucky phases");
    solver_.connect_terminator(&terminator_);

    // The Hadeler traversal has no empty support.
    for (size_t index = 0; index < dimension_; ++index) {
      solver_.phase(-variable(index));
      solver_.add(variable(index));
    }
    solver_.add(0);
  }

  bool take_first(std::vector<size_t> &indices) {
    const int status = solver_.solve();
    if (status == CaDiCaL::UNSATISFIABLE)
      return false;
    if (status != CaDiCaL::SATISFIABLE) {
      timeout_checkpoint();
      throw std::runtime_error(
          "CaDiCaL returned an inconclusive result without a coposit timeout");
    }

    indices.clear();
    for (size_t index = 0; index < dimension_; ++index)
      if (solver_.val(variable(index)) > 0)
        indices.push_back(index);
    return true;
  }

  bool available(const std::vector<size_t> &indices) {
    size_t selected = 0;
    for (size_t index = 0; index < dimension_; ++index) {
      const bool contains =
          selected < indices.size() && indices[selected] == index;
      solver_.assume(contains ? variable(index) : -variable(index));
      selected += contains;
    }
    assert(selected == indices.size());
    const int status = solver_.solve();
    if (status == CaDiCaL::UNSATISFIABLE)
      return false;
    if (status == CaDiCaL::SATISFIABLE)
      return true;
    timeout_checkpoint();
    throw std::runtime_error("CaDiCaL returned an inconclusive availability "
                             "result without a coposit timeout");
  }

  void add_interval(const support &lower, const support &upper) {
    for (size_t index = 0; index < dimension_; ++index) {
      if (context_.contains(lower, index))
        solver_.add(-variable(index));
      else if (!context_.contains(upper, index))
        solver_.add(variable(index));
    }
    solver_.add(0);
  }

  void add_pair_upward_closure(size_t first, size_t second) {
    solver_.add(-variable(first));
    solver_.add(-variable(second));
    solver_.add(0);
  }

  void add_upward_closure(const std::vector<size_t> &indices) {
    for (const size_t index : indices)
      solver_.add(-variable(index));
    solver_.add(0);
  }

  void add_downward_closure(const std::vector<size_t> &indices) {
    size_t selected = 0;
    for (size_t index = 0; index < dimension_; ++index) {
      if (selected < indices.size() && indices[selected] == index)
        ++selected;
      else
        solver_.add(variable(index));
    }
    assert(selected == indices.size());
    solver_.add(0);
  }

private:
  int variable(size_t index) const noexcept {
    return static_cast<int>(index) + 1;
  }

  const support_context &context_;
  size_t dimension_;
  timeout_terminator terminator_;
  CaDiCaL::Solver solver_;
};

struct coverage_score {
  size_t width = 0;
  size_t upper_size = 0;
};

struct pending_interval {
  std::vector<size_t> lower;
  std::vector<size_t> upper;
  std::vector<size_t> source;
  std::string_view kind;
  std::string_view frontier;
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
      : support_context_(dimension), factorization_(dimension),
        floating_filter_(dimension), product_(dimension),
        shortlist_limit_(ray_shortlist_limit(dimension, dimension)),
        mode_(mode), diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
    ceiling_indices_.reserve(dimension);
    for (size_t index = 0; index < dimension; ++index)
      ceiling_indices_.push_back(index);
    ray_shortlist_.reserve(shortlist_limit_);
    shortlist_uppers_.reserve(shortlist_limit_);
    for (size_t index = 0; index < shortlist_limit_; ++index)
      shortlist_uppers_.push_back(support_context_.make());
  }

  dickinson_checker(size_t dimension,
                    copositivity_classification &classification)
      : support_context_(dimension), factorization_(dimension),
        floating_filter_(dimension), product_(dimension),
        shortlist_limit_(ray_shortlist_limit(dimension, dimension)),
        mode_(copositivity_mode::copositive), classification_(&classification),
        diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
    ceiling_indices_.reserve(dimension);
    for (size_t index = 0; index < dimension; ++index)
      ceiling_indices_.push_back(index);
    ray_shortlist_.reserve(shortlist_limit_);
    shortlist_uppers_.reserve(shortlist_limit_);
    for (size_t index = 0; index < shortlist_limit_; ++index)
      shortlist_uppers_.push_back(support_context_.make());
  }

  bool check(const matrix_integer &matrix) {
#ifdef COPOSIT_CADICAL_X1_TESTING
    optimized_certificate_count_ = 0;
    combined_ray_sweep_count_ = 0;
    combined_ray_improvement_count_ = 0;
    pair_curvature_exclusion_count_ = 0;
    support_curvature_exclusion_count_ = 0;
    downward_count_ = 0;
#endif
    supports_.emplace(support_context_);
    if (!install_singleton_ceiling_certificates(matrix))
      return finish(false);
    install_pair_curvature_exclusions(matrix);
    floating_filter_.prepare(matrix);

    while (supports_->take_first(indices_)) {
      diagnostics_.stage(indices_.size());
      if (!process_selected_support(matrix))
        return finish(false);
    }
    return finish(true);
  }

#ifdef COPOSIT_CADICAL_X1_TESTING
  bool process_support_for_testing(const matrix_integer &matrix,
                                   const std::vector<size_t> &indices) {
    supports_.emplace(support_context_);
    floating_filter_.prepare(matrix);
    indices_ = indices;
    certificate_frontier_ = "low";
    const bool result = process_subset(matrix, false);
    if (result)
      commit_pending_intervals();
    return result;
  }

  bool ceiling_dickinson_subsumes_upward_for_testing() {
    supports_.emplace(support_context_);
    certificate_frontier_ = "walk";
    buffer_interval({1, 2}, ceiling_indices_, "support_curvature", {1, 2});
    const bool deferred = supports_->available({1, 2});
    const bool pending_covers = !available({1, 2});
    buffer_interval({1}, ceiling_indices_, "dickinson", {0, 1});
    return deferred && pending_covers && pending_intervals_.size() == 1 &&
           pending_intervals_.front().kind == "dickinson";
  }

  bool process_walk_with_upward_closure_for_testing(
      const matrix_integer &matrix, const std::vector<size_t> &seed,
      const std::vector<size_t> &covered_root) {
    supports_.emplace(support_context_);
    supports_->add_upward_closure(covered_root);
    floating_filter_.prepare(matrix);
    return process_frontier_walk(matrix, seed);
  }

  bool check_support_for_testing(const matrix_integer &matrix,
                                 const std::vector<size_t> &indices) {
    support lower = support_context_.make();
    support upper = support_context_.make();
    const bool result =
        optimize_support_for_testing(matrix, indices, lower, upper);
    last_fixed_support_upper_size = 0;
    for (size_t index = 0; index < matrix.rows(); ++index)
      last_fixed_support_upper_size += support_context_.contains(upper, index);
    support_context_.release(std::move(lower));
    support_context_.release(std::move(upper));
    return result;
  }

  bool optimize_support_for_testing(const matrix_integer &matrix,
                                    const std::vector<size_t> &indices,
                                    support &lower, support &upper) {
    optimized_certificate_count_ = 0;
    combined_ray_sweep_count_ = 0;
    combined_ray_improvement_count_ = 0;
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

  void append_support(std::ostringstream &event, const support &indices) const {
    event << '[';
    bool first = true;
    for (size_t index = 0; index < support_context_.dimension(); ++index) {
      if (!support_context_.contains(indices, index))
        continue;
      if (!first)
        event << ',';
      event << index + 1;
      first = false;
    }
    event << ']';
  }

  static bool was_floating_checked(std::string_view frontier) {
    return frontier == "walk" || frontier == "middle";
  }

  void record_interval_certificate(const support &lower, const support &upper,
                                   const std::vector<size_t> &source,
                                   std::string_view frontier) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=cadical_x1 n=" << product_.size()
          << " frontier=" << frontier << " kind=dickinson source=";
    append_indices(event, source);
    event << " coverage=interval lower=";
    append_support(event, lower);
    event << " upper=";
    append_support(event, upper);
    event << " exclude_empty=no floating_checked="
          << (was_floating_checked(frontier) ? "yes" : "no")
          << " exact_checked=yes";
    diagnostics::record_history_event("certificate", event.str());
  }

  void record_closure_certificate(std::string_view kind,
                                  std::string_view coverage,
                                  std::string_view frontier,
                                  const std::vector<size_t> &source) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=cadical_x1 n=" << product_.size()
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
    event << " floating_checked="
          << (was_floating_checked(frontier) ? "yes" : "no")
          << " exact_checked=yes";
    diagnostics::record_history_event("certificate", event.str());
  }

  void record_pair_curvature_certificate(size_t first, size_t second) {
    record_closure_certificate("pair_curvature", "upward", "initial",
                               {first, second});
  }

  static bool subset_of(const std::vector<size_t> &left,
                        const std::vector<size_t> &right) {
    return std::includes(right.begin(), right.end(), left.begin(), left.end());
  }

  static bool subsumes(const pending_interval &outer,
                       const pending_interval &inner) {
    return subset_of(outer.lower, inner.lower) &&
           subset_of(inner.upper, outer.upper);
  }

  static bool contains(const pending_interval &interval,
                       const std::vector<size_t> &indices) {
    return subset_of(interval.lower, indices) &&
           subset_of(indices, interval.upper);
  }

  bool pending_available(const std::vector<size_t> &indices) const {
    return std::none_of(pending_intervals_.begin(), pending_intervals_.end(),
                        [&](const pending_interval &interval) {
                          return contains(interval, indices);
                        });
  }

  bool available(const std::vector<size_t> &indices) {
    if (!pending_available(indices))
      return false;
    return supports_->available(indices);
  }

  void buffer_interval(std::vector<size_t> lower, std::vector<size_t> upper,
                       std::string_view kind,
                       const std::vector<size_t> &source) {
    assert(subset_of(lower, upper));
    pending_interval candidate{std::move(lower), std::move(upper), source, kind,
                               certificate_frontier_};
    if (std::any_of(pending_intervals_.begin(), pending_intervals_.end(),
                    [&](const pending_interval &known) {
                      return subsumes(known, candidate);
                    }))
      return;
    pending_intervals_.erase(std::remove_if(pending_intervals_.begin(),
                                            pending_intervals_.end(),
                                            [&](const pending_interval &known) {
                                              return subsumes(candidate, known);
                                            }),
                             pending_intervals_.end());
    pending_intervals_.push_back(std::move(candidate));
  }

  void commit_pending_intervals() {
    for (const pending_interval &interval : pending_intervals_) {
      support lower = support_context_.make();
      support upper = support_context_.make();
      for (const size_t index : interval.lower)
        support_context_.set(lower, index);
      for (const size_t index : interval.upper)
        support_context_.set(upper, index);
      supports_->add_interval(lower, upper);
      if (diagnostics_.active())
        diagnostics_.certificate(interval.upper.size() - interval.lower.size(),
                                 interval.upper.size());
      if (interval.kind == "dickinson") {
        record_interval_certificate(lower, upper, interval.source,
                                    interval.frontier);
        COPOSIT_CADICAL_X1_DIAGNOSTICS(interval.upper.size() == product_.size()
                                           ? "dickinson_ceiling"
                                           : "dickinson_interval",
                                       interval.source.size());
      } else {
        const bool downward = interval.lower.empty();
        record_closure_certificate(interval.kind,
                                   downward ? "downward" : "upward",
                                   interval.frontier, interval.source);
        COPOSIT_CADICAL_X1_DIAGNOSTICS(downward ? "downward" : "support_upward",
                                       interval.source.size());
      }
      support_context_.release(std::move(lower));
      support_context_.release(std::move(upper));
    }
    pending_intervals_.clear();
  }

  void record_cadical_seed(const std::vector<size_t> &seed) const {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=cadical_x1 n=" << product_.size()
          << " cardinality=" << seed.size() << " support=";
    append_indices(event, seed);
    diagnostics::record_history_event("cadical_seed", event.str());
  }

  bool process_selected_support(const matrix_integer &matrix) {
    timeout_checkpoint();
    certificate_frontier_ = "low";
    record_cadical_seed(indices_);
    COPOSIT_CADICAL_X1_DIAGNOSTICS("process_seed", indices_.size());
    return process_frontier_walk(matrix, indices_);
  }

  static void add_index(std::vector<size_t> &indices, size_t index) {
    indices.insert(std::lower_bound(indices.begin(), indices.end(), index),
                   index);
  }

  enum class floating_region { downward, middle, upward };

  struct scored_extension {
    size_t candidate = 0;
    double pivot = 0.0;
  };

  struct extension_choice {
    size_t index = 0;
    bool available = false;
    floating_region region = floating_region::middle;
  };

  struct chain_point {
    size_t added_index = 0;
    bool available = false;
    floating_region region = floating_region::middle;
  };

  std::optional<size_t>
  first_available_extension(std::vector<size_t> &extension,
                            const std::vector<size_t> &ranked_candidates) {
    for (const size_t candidate : ranked_candidates) {
      const auto position = extension.insert(
          std::lower_bound(extension.begin(), extension.end(), candidate),
          candidate);
      const bool candidate_available = available(extension);
      extension.erase(position);
      if (candidate_available)
        return candidate;
    }
    return std::nullopt;
  }

  extension_choice best_chain_extension(const std::vector<size_t> &current,
                                        const std::vector<size_t> &candidates) {
    assert(!candidates.empty());
    std::vector<size_t> extension = current;
    extension.reserve(product_.size());

    const bool principal_positive =
        floating_filter_.prepare_principal_extensions(current);
    std::vector<bool> principal_candidate(candidates.size(), false);
    std::vector<scored_extension> principal_ranked;
    if (principal_positive) {
      principal_ranked.reserve(candidates.size());
      for (size_t position = 0; position < candidates.size(); ++position) {
        const size_t candidate = candidates[position];
        const auto score = floating_filter_.score_extension(candidate);
        if (!score || score->pivot <= score->tolerance)
          continue;
        principal_candidate[position] = true;
        principal_ranked.push_back({candidate, score->pivot});
      }
      std::sort(
          principal_ranked.begin(), principal_ranked.end(),
          [](const scored_extension &left, const scored_extension &right) {
            return left.pivot > right.pivot ||
                   (left.pivot == right.pivot &&
                    left.candidate < right.candidate);
          });
      std::vector<size_t> ranked;
      ranked.reserve(principal_ranked.size());
      for (const scored_extension &candidate : principal_ranked)
        ranked.push_back(candidate.candidate);
      if (const auto selected = first_available_extension(extension, ranked))
        return {*selected, true, floating_region::downward};
    }

    std::vector<size_t> remaining;
    remaining.reserve(candidates.size() - principal_ranked.size());
    for (size_t position = 0; position < candidates.size(); ++position)
      if (!principal_candidate[position])
        remaining.push_back(candidates[position]);

    const bool reduced_positive =
        !current.empty() &&
        floating_filter_.prepare_reduced_hessian_extensions(current);
    std::vector<scored_extension> reduced_ranked;
    std::vector<size_t> unscored;
    if (reduced_positive) {
      reduced_ranked.reserve(remaining.size());
      unscored.reserve(remaining.size());
      for (const size_t candidate : remaining) {
        const auto score = floating_filter_.score_extension(candidate);
        if (score)
          reduced_ranked.push_back({candidate, score->pivot});
        else
          unscored.push_back(candidate);
      }
      std::sort(
          reduced_ranked.begin(), reduced_ranked.end(),
          [](const scored_extension &left, const scored_extension &right) {
            return left.pivot < right.pivot ||
                   (left.pivot == right.pivot &&
                    left.candidate < right.candidate);
          });
      std::vector<size_t> ranked;
      ranked.reserve(reduced_ranked.size());
      for (const scored_extension &candidate : reduced_ranked)
        ranked.push_back(candidate.candidate);
      if (const auto selected = first_available_extension(extension, ranked))
        return {*selected, true,
                principal_positive ? floating_region::downward
                                   : floating_region::middle};
      if (const auto selected = first_available_extension(extension, unscored))
        return {*selected, true,
                principal_positive ? floating_region::downward
                                   : floating_region::middle};
    } else if (const auto selected =
                   first_available_extension(extension, remaining)) {
      return {*selected, true,
              principal_positive ? floating_region::downward
                                 : floating_region::upward};
    }

    const size_t fallback =
        !principal_ranked.empty()
            ? principal_ranked.front().candidate
            : (!reduced_ranked.empty() ? reduced_ranked.front().candidate
                                       : candidates.front());
    return {fallback, false,
            principal_positive ? floating_region::downward
                               : (reduced_positive ? floating_region::middle
                                                   : floating_region::upward)};
  }

  void append_chain_segment(std::vector<size_t> &current,
                            std::vector<size_t> candidates,
                            std::vector<chain_point> &chain) {
    while (!candidates.empty()) {
      timeout_checkpoint();
      const extension_choice next = best_chain_extension(current, candidates);
      if (!current.empty())
        chain.back().region = next.region;
      add_index(current, next.index);
      chain.push_back({next.index, next.available, floating_region::middle});
      candidates.erase(
          std::lower_bound(candidates.begin(), candidates.end(), next.index));
    }
  }

  std::vector<chain_point> build_chain(const std::vector<size_t> &seed) {
    std::vector<chain_point> chain;
    chain.reserve(product_.size());
    std::vector<size_t> current;
    current.reserve(product_.size());
    append_chain_segment(current, seed, chain);

    std::vector<size_t> outside;
    outside.reserve(product_.size() - seed.size());
    for (size_t index = 0; index < product_.size(); ++index)
      if (!std::binary_search(seed.begin(), seed.end(), index))
        outside.push_back(index);
    append_chain_segment(current, std::move(outside), chain);
    if (!chain.empty() && chain.back().available)
      chain.back().region = classify_floating_region(current);
    return chain;
  }

  floating_region classify_floating_region(const std::vector<size_t> &indices) {
    if (floating_filter_.prepare_principal_extensions(indices))
      return floating_region::downward;
    if (floating_filter_.looks_reduced_hessian_positive_definite(indices))
      return floating_region::middle;
    return floating_region::upward;
  }

  bool process_frontier_walk(const matrix_integer &matrix,
                             const std::vector<size_t> &seed) {
    assert(pending_intervals_.empty());
    const std::vector<chain_point> chain = build_chain(seed);
    std::vector<size_t> current;
    current.reserve(product_.size());
    std::optional<std::vector<size_t>> downward_candidate;
    std::optional<std::vector<size_t>> upward_candidate;

    for (const chain_point &point : chain) {
      add_index(current, point.added_index);
      if (!point.available)
        continue;
      if (point.region == floating_region::downward && !upward_candidate)
        downward_candidate = current;
      else if (point.region == floating_region::upward && !upward_candidate)
        upward_candidate = current;
    }

    certificate_frontier_ = "walk";
    if (downward_candidate)
      verify_downward_frontier(matrix, *downward_candidate);
    if (upward_candidate)
      verify_upward_frontier(matrix, *upward_candidate);

    size_t exact_bridge_supports = 0;
    current.clear();
    for (const chain_point &point : chain) {
      timeout_checkpoint();
      add_index(current, point.added_index);
      if (!point.available || !pending_available(current))
        continue;
      indices_ = current;
      certificate_frontier_ = "middle";
      diagnostics_.visit_support();
      diagnostics_.secondary();
      ++exact_bridge_supports;
      if (!process_subset(matrix, true))
        return false;
      if (available(current))
        throw std::logic_error(
            "exact chain processing did not cover its generating support");
    }

    record_walk(chain.size() + 1, exact_bridge_supports, downward_candidate,
                upward_candidate);
    commit_pending_intervals();
    if (supports_->available(seed))
      throw std::logic_error(
          "a completed CaDiCaL X1 pass left its seed uncovered");
    return true;
  }

  void record_walk(size_t steps, size_t exact_bridge_supports,
                   const std::optional<std::vector<size_t>> &downward,
                   const std::optional<std::vector<size_t>> &upward) const {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=cadical_x1 n=" << product_.size() << " steps=" << steps
          << " exact_bridge_supports=" << exact_bridge_supports;
    if (downward) {
      event << " downward=";
      append_indices(event, *downward);
    }
    if (upward) {
      event << " upward=";
      append_indices(event, *upward);
    }
    diagnostics::record_history_event("frontier_walk", event.str());
  }

  void verify_downward_frontier(const matrix_integer &matrix,
                                const std::vector<size_t> &candidate) {
    diagnostics_.visit_support();
    diagnostics_.secondary();
    indices_ = candidate;
    principal_.resize(indices_.size(), indices_.size());
    solution_.resize(indices_.size(), 1);
    copy_principal(matrix, indices_, principal_);
    const bool nonsingular = factorization_.factorize_inplace(principal_) != 0;
    if (nonsingular && factorization_.is_positive_definite())
      add_downward_closure("frontier_positive_definite");
    else if (!nonsingular)
      static_cast<void>(add_singular_psd_downward_closure());
  }

  void verify_upward_frontier(const matrix_integer &matrix,
                              const std::vector<size_t> &candidate) {
    diagnostics_.visit_support();
    diagnostics_.secondary();
    indices_ = candidate;
    principal_.resize(indices_.size(), indices_.size());
    solution_.resize(indices_.size(), 1);
    copy_principal(matrix, indices_, principal_);
    const bool singular = factorization_.factorize_inplace(principal_) == 0;
    bool positive_definite = false;
    if (singular) {
      factorization_.one_nullspace_vector(solution_, principal_);
      positive_definite = singular_reduced_hessian_is_positive_definite();
    } else {
      for (size_t row = 0; row < indices_.size(); ++row)
        solution_(row, 0).set_one();
      integer denominator;
      factorization_.solve_inplace(solution_, denominator, principal_);
      positive_definite = nonsingular_reduced_hessian_is_positive_definite();
    }
    if (!positive_definite)
      add_upward_closure();
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
        COPOSIT_CADICAL_X1_DIAGNOSTICS("pair_upward", 2);
#ifdef COPOSIT_CADICAL_X1_TESTING
        ++pair_curvature_exclusion_count_;
#endif
      }
    }
  }

  bool install_singleton_ceiling_certificates(const matrix_integer &matrix) {
    assert(pending_intervals_.empty());
    certificate_frontier_ = "initial";
    indices_.resize(1);
    for (size_t index = 0; index < matrix.rows(); ++index) {
      timeout_checkpoint();
      indices_[0] = index;
      diagnostics_.visit_support();
      if (!process_subset(matrix))
        return false;
      pending_intervals_.erase(
          std::remove_if(pending_intervals_.begin(), pending_intervals_.end(),
                         [&](const pending_interval &interval) {
                           return interval.kind == "dickinson" &&
                                  interval.upper.size() != matrix.rows();
                         }),
          pending_intervals_.end());
    }
    commit_pending_intervals();
    return true;
  }

  bool process_subset(const matrix_integer &matrix,
                      bool allow_downward = false) {
    const size_t dimension = indices_.size();
    principal_.resize(dimension, dimension);
    solution_.resize(dimension, 1);
    copy_principal(matrix, indices_, principal_);

    const bool singular = factorization_.factorize_inplace(principal_) == 0;
    if (singular && diagnostics_.active())
      diagnostics_.singular_support(dimension - factorization_.rank());
    if (allow_downward && singular && add_singular_psd_downward_closure())
      return true;
    if (allow_downward && !singular && factorization_.is_positive_definite()) {
      add_downward_closure("positive_definite");
      return true;
    }
    if (singular)
      return process_singular_subset(matrix);
    return process_nonsingular_subset(matrix);
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
    }

    return add_certificate();
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
#ifdef COPOSIT_CADICAL_X1_TESTING
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
    support_context_.clear(upper);
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
        support_context_.set(upper, row);
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
      const bool first_upper =
          support_context_.contains(shortlist_uppers_[first], row);
      const bool second_upper =
          support_context_.contains(shortlist_uppers_[second], row);
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
#ifdef COPOSIT_CADICAL_X1_TESTING
      ++combined_ray_sweep_count_;
#endif
      COPOSIT_CADICAL_X1_DIAGNOSTICS("combined_ray", ray + 1);
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
#ifdef COPOSIT_CADICAL_X1_TESTING
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
    support lower = support_context_.make();
    support upper = support_context_.make();
    std::vector<size_t> lower_indices;
    std::vector<size_t> upper_indices;
    for (size_t local = 0; local < indices_.size(); ++local) {
      if (!solution_(local, 0).is_zero()) {
        support_context_.set(lower, indices_[local]);
        lower_indices.push_back(indices_[local]);
      }
    }
    for (size_t row = 0; row < product_.size(); ++row) {
      if (product_[row].sign() >= 0) {
        support_context_.set(upper, row);
        upper_indices.push_back(row);
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
      else if (mode_ == copositivity_mode::strictly_copositive) {
        support_context_.release(std::move(lower));
        support_context_.release(std::move(upper));
        return false;
      }
    }

#ifdef COPOSIT_CADICAL_X1_TESTING
    if (captured_lower_ != nullptr) {
      support_context_.copy(*captured_lower_, lower);
      support_context_.copy(*captured_upper_, upper);
      support_context_.release(std::move(lower));
      support_context_.release(std::move(upper));
      return true;
    }
#endif
    buffer_interval(std::move(lower_indices), std::move(upper_indices),
                    "dickinson", indices_);
    support_context_.release(std::move(lower));
    support_context_.release(std::move(upper));
    return true;
  }

  bool add_curvature_exclusion() {
#ifdef COPOSIT_CADICAL_X1_TESTING
    ++support_curvature_exclusion_count_;
    if (captured_lower_ != nullptr) {
      support lower = support_context_.make();
      support ceiling = support_context_.make();
      for (const size_t index : indices_)
        support_context_.set(lower, index);
      support_context_.set_all(ceiling);
      support_context_.copy(*captured_lower_, lower);
      support_context_.copy(*captured_upper_, ceiling);
      support_context_.release(std::move(lower));
      support_context_.release(std::move(ceiling));
      return true;
    }
#endif
    add_upward_closure();
    return true;
  }

  void add_upward_closure() {
    buffer_interval(indices_, ceiling_indices_, "support_curvature", indices_);
  }

  void add_downward_closure(std::string_view kind) {
    buffer_interval({}, indices_, kind, indices_);
#ifdef COPOSIT_CADICAL_X1_TESTING
    ++downward_count_;
#endif
  }

  bool finish(bool result) {
    diagnostics_.finish();
#ifdef COPOSIT_CADICAL_X1_TESTING
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

#ifdef COPOSIT_CADICAL_X1_TESTING
  void publish_test_counters() const noexcept {
    last_optimized_certificate_count = optimized_certificate_count_;
    last_combined_ray_sweep_count = combined_ray_sweep_count_;
    last_combined_ray_improvement_count = combined_ray_improvement_count_;
    last_pair_curvature_exclusion_count = pair_curvature_exclusion_count_;
    last_support_curvature_exclusion_count = support_curvature_exclusion_count_;
    last_downward_count = downward_count_;
  }
#endif

  support_context support_context_;
  fraction_free_ldlt_factorization factorization_;
  floating_positive_semidefinite_filter floating_filter_;
  matrix_integer principal_;
  matrix_integer solution_;
  matrix_integer directions_;
  matrix_integer direction_products_;
  matrix_integer combined_directions_;
  matrix_integer combined_products_;
  std::vector<integer> product_;
  size_t shortlist_limit_;
  std::vector<size_t> indices_;
  std::vector<size_t> ceiling_indices_;
  std::vector<pending_interval> pending_intervals_;
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
  std::optional<interval_sat> supports_;
#ifdef COPOSIT_CADICAL_X1_TESTING
  size_t optimized_certificate_count_ = 0;
  size_t combined_ray_sweep_count_ = 0;
  size_t combined_ray_improvement_count_ = 0;
  size_t pair_curvature_exclusion_count_ = 0;
  size_t support_curvature_exclusion_count_ = 0;
  size_t downward_count_ = 0;
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

#ifdef COPOSIT_CADICAL_X1_TESTING
bool cadical_x1_prefers_negative_singular_orientation_for_testing(
    size_t positive_products, size_t negative_products) noexcept {
  return negative_orientation_has_larger_upper(positive_products,
                                               negative_products);
}

size_t cadical_x1_optimized_certificate_count_for_testing() noexcept {
  return last_optimized_certificate_count;
}

size_t cadical_x1_combined_ray_sweep_count_for_testing() noexcept {
  return last_combined_ray_sweep_count;
}

size_t cadical_x1_combined_ray_improvement_count_for_testing() noexcept {
  return last_combined_ray_improvement_count;
}

size_t cadical_x1_pair_curvature_exclusion_count_for_testing() noexcept {
  return last_pair_curvature_exclusion_count;
}

size_t cadical_x1_support_curvature_exclusion_count_for_testing() noexcept {
  return last_support_curvature_exclusion_count;
}

size_t cadical_x1_pair_upward_count_for_testing() noexcept {
  return last_pair_curvature_exclusion_count;
}

size_t cadical_x1_support_upward_count_for_testing() noexcept {
  return last_support_curvature_exclusion_count;
}

size_t cadical_x1_downward_count_for_testing() noexcept {
  return last_downward_count;
}

bool cadical_x1_reduced_hessian_is_positive_definite_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .reduced_hessian_is_positive_definite_for_testing(matrix, indices);
}

size_t cadical_x1_shortlist_limit_for_testing(size_t matrix_dimension,
                                              size_t support_dimension) {
  return ray_shortlist_limit(matrix_dimension, support_dimension);
}

bool cadical_x1_prefers_ray_candidate_for_testing(
    size_t candidate_upper, size_t candidate_width, size_t candidate_gains,
    size_t candidate_losses, size_t current_upper, size_t current_width,
    size_t current_gains, size_t current_losses) {
  return better_ray_candidate(
      {candidate_width, candidate_upper}, candidate_gains, candidate_losses,
      true, {current_width, current_upper}, current_gains, current_losses);
}

bool cadical_x1_check_support_for_testing(const matrix_integer &matrix,
                                          const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(),
                           copositivity_mode::strictly_copositive)
      .check_support_for_testing(matrix, indices);
}

bool cadical_x1_process_support_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .process_support_for_testing(matrix, indices);
}

bool cadical_x1_ceiling_dickinson_subsumes_upward_for_testing() {
  return dickinson_checker(3, copositivity_mode::copositive)
      .ceiling_dickinson_subsumes_upward_for_testing();
}

bool cadical_x1_process_walk_with_upward_closure_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &seed,
    const std::vector<size_t> &covered_root) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .process_walk_with_upward_closure_for_testing(matrix, seed, covered_root);
}

bool cadical_x1_certificate_for_testing(const matrix_integer &matrix,
                                        const std::vector<size_t> &indices,
                                        support &lower, support &upper) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .optimize_support_for_testing(matrix, indices, lower, upper);
}

size_t cadical_x1_fixed_support_upper_size_for_testing() noexcept {
  return last_fixed_support_upper_size;
}

#endif

} // namespace coposit::model
