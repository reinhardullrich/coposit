#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/improved_nbc_upward_supports.hpp>
#include <coposit/milp_solver.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
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

#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
size_t last_pair_curvature_exclusion_count = 0;
size_t last_support_curvature_exclusion_count = 0;
size_t last_downward_count = 0;
size_t last_high_float_rejection_count = 0;
size_t last_root_relaxation_attempt_count = 0;
size_t last_root_relaxation_improvement_count = 0;
size_t last_root_relaxation_node_count = 0;
#endif

struct coverage_score {
  size_t width = 0;
  size_t upper_size = 0;
};

bool better_score(const coverage_score &candidate,
                  const coverage_score &current) noexcept {
  return candidate.width > current.width ||
         (candidate.width == current.width &&
          candidate.upper_size > current.upper_size);
}

class dickinson_checker {
public:
  dickinson_checker(size_t dimension, copositivity_mode mode)
      : support_context_(dimension), factorization_(dimension),
        floating_filter_(dimension), product_(dimension),
        mode_(mode), diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
  }

  dickinson_checker(size_t dimension,
                    copositivity_classification &classification)
      : support_context_(dimension), factorization_(dimension),
        floating_filter_(dimension), product_(dimension),
        mode_(copositivity_mode::copositive), classification_(&classification),
        diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
  }

  bool check(const matrix_integer &matrix) {
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
    pair_curvature_exclusion_count_ = 0;
    support_curvature_exclusion_count_ = 0;
    downward_count_ = 0;
    high_float_rejection_count_ = 0;
    root_relaxation_attempt_count_ = 0;
    root_relaxation_improvement_count_ = 0;
    root_relaxation_node_count_ = 0;
#endif
    supports_.emplace(support_context_);
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

#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
  bool check_support_for_testing(const matrix_integer &matrix,
                                 const std::vector<size_t> &indices) {
    support lower = support_context_.make();
    support upper = support_context_.make();
    const bool result =
        optimize_support_for_testing(matrix, indices, lower, upper);
    support_context_.release(std::move(lower));
    support_context_.release(std::move(upper));
    return result;
  }

  bool optimize_support_for_testing(const matrix_integer &matrix,
                                    const std::vector<size_t> &indices,
                                    support &lower, support &upper) {
    root_relaxation_attempt_count_ = 0;
    root_relaxation_improvement_count_ = 0;
    root_relaxation_node_count_ = 0;
    indices_ = indices;
    captured_lower_ = &lower;
    captured_upper_ = &upper;
    const bool result = process_subset(matrix);
    captured_lower_ = nullptr;
    captured_upper_ = nullptr;
    publish_test_counters();
    return result;
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

  void record_interval_certificate(const support &lower, const support &upper) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=improved_nbc_x8 n=" << product_.size()
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
    event << "model=improved_nbc_x8 n=" << product_.size()
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
    event << "model=improved_nbc_x8 n=" << product_.size()
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
      COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS("stage_low", low);
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
      COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS("stage_high", high);
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
    COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS(
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
        COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS("pair_upward", 2);
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
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
        COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS("high_float_reject",
                                            indices_.size());
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
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
    if (current_score_.upper_size == matrix.rows())
      return add_certificate();

    directions_.resize(dimension, dimension);
    for (size_t row = 0; row < dimension; ++row)
      for (size_t column = 0; column < dimension; ++column) {
        if (row == column)
          directions_(row, column).set_one();
        else
          directions_(row, column).set_zero();
      }

    integer direction_denominator;
    factorization_.solve_inplace(directions_, direction_denominator, principal_);
    assert(direction_denominator.compare(denominator) == 0);
    direction_products_.resize(matrix.rows(), dimension);
    for (size_t direction = 0; direction < dimension; ++direction)
      calculate_nonsingular_product(matrix, directions_, direction,
                                    direction_denominator, direction_products_,
                                    direction);

    if (!optimize_with_root_relaxation())
      return false;
    return add_certificate();
  }

  bool optimize_with_root_relaxation() {
    timeout_checkpoint();
    const size_t support_dimension = indices_.size();
    const size_t outside_dimension = product_.size() - support_dimension;
    assert(outside_dimension > 0);

    const size_t primary_weight = outside_dimension + 1;
    const size_t outside_weight = primary_weight + 1;
    std::vector<std::vector<double>> rows;
    std::vector<size_t> row_weights;
    rows.reserve(outside_dimension + 2 * support_dimension);
    row_weights.reserve(rows.capacity());
    const auto append_scaled_row = [&](const matrix_integer &source,
                                       size_t source_row, int orientation,
                                       size_t weight) {
      rows.emplace_back(support_dimension);
      std::vector<double> &scaled = rows.back();
      slong maximum_exponent = std::numeric_limits<slong>::min();
      for (size_t column = 0; column < support_dimension; ++column) {
        if (source(source_row, column).is_zero())
          continue;
        slong exponent = 0;
        static_cast<void>(source(source_row, column).to_dbl_2exp(exponent));
        maximum_exponent = std::max(maximum_exponent, exponent);
      }
      if (maximum_exponent != std::numeric_limits<slong>::min()) {
        for (size_t column = 0; column < support_dimension; ++column) {
          if (source(source_row, column).is_zero())
            continue;
          slong exponent = 0;
          const double mantissa = source(source_row, column).to_dbl_2exp(exponent);
          const slong difference = exponent - maximum_exponent;
          scaled[column] =
              difference < -1074
                  ? 0.0
                  : static_cast<double>(orientation) *
                        std::scalbn(mantissa, static_cast<int>(difference));
        }
      }
      row_weights.push_back(weight);
    };

    size_t outside = 0;
    size_t local = 0;
    for (size_t row = 0; row < product_.size(); ++row) {
      if (local < indices_.size() && row == indices_[local]) {
        ++local;
        continue;
      }
      append_scaled_row(direction_products_, row, 1, outside_weight);
      if ((++outside & 63U) == 0)
        timeout_checkpoint();
    }
    for (size_t row = 0; row < support_dimension; ++row) {
      append_scaled_row(directions_, row, 1, primary_weight);
      append_scaled_row(directions_, row, -1, primary_weight);
    }

    const size_t outside_incumbent =
        current_score_.upper_size - support_dimension;
    const size_t incumbent_objective =
        primary_weight * (support_dimension + current_score_.width) +
        outside_incumbent;
    const size_t objective_ceiling =
        primary_weight * (support_dimension + product_.size() - 1) +
        outside_dimension;
    maximum_halfspaces_milp_solver optimizer(
        rows, std::move(row_weights), 0.0, incumbent_objective,
        1,
        std::chrono::steady_clock::time_point::max(), objective_ceiling);
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
    ++root_relaxation_attempt_count_;
#endif
    auto result = optimizer.solve();
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
    root_relaxation_node_count_ += result.nodes;
#endif
    timeout_checkpoint();
    if (!result.optimal && !result.root_relaxation_solved)
      throw std::runtime_error("Dickinson root relaxation did not complete");
    COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS("root_relaxation", indices_.size());
    if (result.point.empty())
      return true;

    lp_coefficients_.resize(support_dimension);
    lp_solution_.resize(support_dimension, 1);
    lp_product_.resize(product_.size());
    const double maximum =
        *std::max_element(result.point.begin(), result.point.end());
    if (!(maximum > 0.0) || !std::isfinite(maximum))
      return true;
    constexpr uint64_t scale = 1'000'000'000ULL;
    for (size_t column = 0; column < support_dimension; ++column) {
      const double normalized = result.point[column] / maximum;
      if (!(normalized >= 0.0) || !std::isfinite(normalized))
        return true;
      const uint64_t coefficient =
          static_cast<uint64_t>(std::llround(normalized * scale));
      fmpz_set_ui(lp_coefficients_[column].native_handle(),
                  static_cast<ulong>(coefficient));
    }

    multiply_directions(directions_, direction_products_, lp_coefficients_,
                        lp_solution_, lp_product_);
    if (all_nonpositive(lp_solution_, 0))
      return true;
    const coverage_score candidate_score =
        score(lp_solution_, 0, lp_product_);
    if (!better_score(candidate_score, current_score_))
      return true;

    solution_ = lp_solution_;
    product_ = lp_product_;
    current_score_ = candidate_score;
    remove_common_content();
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
    ++root_relaxation_improvement_count_;
#endif
    COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS("root_relaxation_improvement",
                                        indices_.size());
    return true;
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
    size_t lower_size = 0;
    size_t upper_size = 0;
    for (size_t local = 0; local < indices_.size(); ++local) {
      if (!solution_(local, 0).is_zero()) {
        support_context_.set(lower, indices_[local]);
        ++lower_size;
      }
    }
    for (size_t row = 0; row < product_.size(); ++row) {
      if (product_[row].sign() >= 0) {
        support_context_.set(upper, row);
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
      else if (mode_ == copositivity_mode::strictly_copositive) {
        support_context_.release(std::move(lower));
        support_context_.release(std::move(upper));
        return false;
      }
    }

#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
    if (captured_lower_ != nullptr) {
      support_context_.copy(*captured_lower_, lower);
      support_context_.copy(*captured_upper_, upper);
      support_context_.release(std::move(lower));
      support_context_.release(std::move(upper));
      return true;
    }
#endif
    supports_->add_interval(lower, upper);
    if (diagnostics_.active())
      diagnostics_.certificate(upper_size - lower_size, upper_size);
    record_interval_certificate(lower, upper);
    support_context_.release(std::move(lower));
    support_context_.release(std::move(upper));
    COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS("dickinson", indices_.size());
    return true;
  }

  bool add_curvature_exclusion() {
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
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
    support lower = support_context_.make();
    support ceiling = support_context_.make();
    for (const size_t index : indices_)
      support_context_.set(lower, index);
    support_context_.set_all(ceiling);
    supports_->add_interval(lower, ceiling);
    support_context_.release(std::move(lower));
    support_context_.release(std::move(ceiling));
    if (diagnostics_.active())
      diagnostics_.certificate(product_.size() - indices_.size(),
                               product_.size());
    record_closure_certificate("support_curvature", "upward",
                               certificate_frontier_, indices_);
    COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS("support_upward", indices_.size());
  }

  void add_downward_closure(std::string_view kind) {
    support floor = support_context_.make();
    support upper = support_context_.make();
    for (const size_t index : indices_)
      support_context_.set(upper, index);
    supports_->add_interval(floor, upper);
    support_context_.release(std::move(floor));
    support_context_.release(std::move(upper));
    diagnostics_.certificate();
    record_closure_certificate(kind, "downward", certificate_frontier_,
                               indices_);
    COPOSIT_IMPROVED_NBC_X8_DIAGNOSTICS("downward", indices_.size());
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
    ++downward_count_;
#endif
  }

  bool finish(bool result) {
    diagnostics_.finish();
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
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

#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
  void publish_test_counters() const noexcept {
    last_pair_curvature_exclusion_count = pair_curvature_exclusion_count_;
    last_support_curvature_exclusion_count = support_curvature_exclusion_count_;
    last_downward_count = downward_count_;
    last_high_float_rejection_count = high_float_rejection_count_;
    last_root_relaxation_attempt_count = root_relaxation_attempt_count_;
    last_root_relaxation_improvement_count = root_relaxation_improvement_count_;
    last_root_relaxation_node_count = root_relaxation_node_count_;
  }
#endif

  support_context support_context_;
  fraction_free_ldlt_factorization factorization_;
  floating_positive_semidefinite_filter floating_filter_;
  matrix_integer principal_;
  matrix_integer solution_;
  matrix_integer directions_;
  matrix_integer direction_products_;
  matrix_integer lp_solution_;
  std::vector<integer> product_;
  std::vector<integer> lp_product_;
  std::vector<integer> lp_coefficients_;
  std::vector<size_t> indices_;
  coverage_score current_score_;
  std::string_view certificate_frontier_ = "direct";
  const copositivity_mode mode_;
  copositivity_classification *classification_ = nullptr;
  diagnostics::tracker diagnostics_;
  std::optional<improved_nbc_upward_supports> supports_;
#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
  size_t pair_curvature_exclusion_count_ = 0;
  size_t support_curvature_exclusion_count_ = 0;
  size_t downward_count_ = 0;
  size_t high_float_rejection_count_ = 0;
  size_t root_relaxation_attempt_count_ = 0;
  size_t root_relaxation_improvement_count_ = 0;
  size_t root_relaxation_node_count_ = 0;
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

#ifdef COPOSIT_IMPROVED_NBC_X8_TESTING
size_t improved_nbc_x8_pair_upward_count_for_testing() noexcept {
  return last_pair_curvature_exclusion_count;
}

size_t improved_nbc_x8_support_upward_count_for_testing() noexcept {
  return last_support_curvature_exclusion_count;
}

size_t improved_nbc_x8_downward_count_for_testing() noexcept {
  return last_downward_count;
}

size_t improved_nbc_x8_high_float_rejection_count_for_testing() noexcept {
  return last_high_float_rejection_count;
}

size_t improved_nbc_x8_root_relaxation_attempt_count_for_testing() noexcept {
  return last_root_relaxation_attempt_count;
}

size_t improved_nbc_x8_root_relaxation_improvement_count_for_testing() noexcept {
  return last_root_relaxation_improvement_count;
}

size_t improved_nbc_x8_root_relaxation_node_count_for_testing() noexcept {
  return last_root_relaxation_node_count;
}

bool improved_nbc_x8_floating_psd_candidate_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  floating_positive_semidefinite_filter filter(matrix.rows());
  filter.prepare(matrix);
  return filter.looks_positive_semidefinite(indices);
}

bool improved_nbc_x8_check_support_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(),
                           copositivity_mode::strictly_copositive)
      .check_support_for_testing(matrix, indices);
}

bool improved_nbc_x8_certificate_for_testing(const matrix_integer &matrix,
                                             const std::vector<size_t> &indices,
                                             support &lower, support &upper) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .optimize_support_for_testing(matrix, indices, lower, upper);
}

bool count_uncovered_support(void *opaque, const std::vector<size_t> &) {
  ++*static_cast<size_t *>(opaque);
  return true;
}

size_t improved_nbc_x8_uncovered_count(
    size_t dimension, size_t cardinality,
    const std::vector<std::pair<uint64_t, uint64_t>> &intervals) {
  support_context context(dimension);
  improved_nbc_upward_supports diagram(context);
  for (const auto &[lower_mask, upper_mask] : intervals) {
    support lower = context.make();
    support upper = context.make();
    for (size_t bit = 0; bit < dimension; ++bit) {
      if ((lower_mask & (uint64_t{1} << bit)) != 0)
        context.set(lower, bit);
      if ((upper_mask & (uint64_t{1} << bit)) != 0)
        context.set(upper, bit);
    }
    diagram.add_interval(lower, upper);
    context.release(std::move(lower));
    context.release(std::move(upper));
  }

  diagram.commit_frontiers(1, dimension);
  size_t count = 0;
  const auto result = diagram.enumerate_cardinality(cardinality, &count,
                                                    &count_uncovered_support);
  assert(result == improved_nbc_upward_supports::enumeration_result::exhausted);
  return count;
}

size_t improved_nbc_x8_uncovered_count(
    size_t dimension, size_t cardinality,
    const std::vector<std::vector<size_t>> &upward,
    const std::vector<std::vector<size_t>> &downward,
    const std::vector<std::vector<size_t>> &exact) {
  support_context context(dimension);
  improved_nbc_upward_supports supports(context);
  support ceiling = context.make();
  context.set_all(ceiling);
  for (const auto &indices : upward) {
    support lower = context.make();
    for (const size_t index : indices)
      context.set(lower, index);
    supports.add_interval(lower, ceiling);
    context.release(std::move(lower));
  }
  for (const auto &indices : downward) {
    support floor = context.make();
    support upper = context.make();
    for (const size_t index : indices)
      context.set(upper, index);
    supports.add_interval(floor, upper);
    context.release(std::move(floor));
    context.release(std::move(upper));
  }
  for (const auto &indices : exact) {
    support singleton = context.make();
    for (const size_t index : indices)
      context.set(singleton, index);
    supports.add_interval(singleton, singleton);
    context.release(std::move(singleton));
  }
  context.release(std::move(ceiling));

  supports.commit_frontiers(1, dimension);
  size_t count = 0;
  const auto result = supports.enumerate_cardinality(cardinality, &count,
                                                     &count_uncovered_support);
  assert(result == improved_nbc_upward_supports::enumeration_result::exhausted);
  return count;
}

size_t improved_nbc_x8_interval_clause_size(size_t dimension,
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
