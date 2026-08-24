#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/improved_nbc_upward_supports.hpp>
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
#include <optional>
#include <random>
#include <set>
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

size_t next_walk_gap(size_t dimension, size_t current_gap,
                     bool reached_kkt) noexcept {
  if (reached_kkt)
    return std::max<size_t>(1, dimension);
  return current_gap <= std::numeric_limits<size_t>::max() / 2
             ? current_gap * 2
             : std::numeric_limits<size_t>::max();
}

class floating_positive_semidefinite_filter {
public:
  enum class reduced_curvature {
    positive_definite,
    positive_semidefinite,
    other
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

  reduced_curvature
  classify_reduced_hessian(const std::vector<size_t> &indices) {
    if (indices.size() < 2)
      return reduced_curvature::positive_definite;

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
    if (scale == 0.0)
      return reduced_curvature::positive_semidefinite;
    if (!std::isfinite(scale))
      return reduced_curvature::other;

    const double tolerance = 64.0 * std::numeric_limits<double>::epsilon() *
                             scale * static_cast<double>(order);
    bool singular = false;
    for (size_t column = 0; column < order; ++column) {
      timeout_checkpoint();
      double pivot = principal_[column * order + column];
      for (size_t previous = 0; previous < column; ++previous) {
        const double multiplier = principal_[column * order + previous];
        pivot -= multiplier * multiplier * diagonal_[previous];
      }
      if (!std::isfinite(pivot) || pivot < -tolerance)
        return reduced_curvature::other;
      if (pivot <= tolerance) {
        singular = true;
        diagonal_[column] = 0.0;
        for (size_t row = column + 1; row < order; ++row) {
          double entry = principal_[row * order + column];
          for (size_t previous = 0; previous < column; ++previous) {
            entry -= principal_[row * order + previous] *
                     principal_[column * order + previous] *
                     diagonal_[previous];
          }
          if (!std::isfinite(entry) || std::abs(entry) > tolerance)
            return reduced_curvature::other;
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
          return reduced_curvature::other;
        principal_[row * order + column] = entry;
      }
    }
    return singular ? reduced_curvature::positive_semidefinite
                    : reduced_curvature::positive_definite;
  }

private:
  size_t dimension_;
  bool prepared_ = false;
  std::vector<double> matrix_;
  std::vector<double> principal_;
  std::vector<double> diagonal_;
};

class double_matrix {
public:
  void resize(size_t rows, size_t columns) {
    columns_ = columns;
    values_.assign(rows * columns, 0.0);
  }

  double &operator()(size_t row, size_t column) noexcept {
    return values_[row * columns_ + column];
  }

  double operator()(size_t row, size_t column) const noexcept {
    return values_[row * columns_ + column];
  }

private:
  size_t columns_ = 0;
  std::vector<double> values_;
};

constexpr double bunch_kaufman_alpha = 0.6403882032022076;
constexpr double floating_pivot_cutoff =
    64.0 * std::numeric_limits<double>::epsilon();

/*
 * Lower-triangle, one-RHS subset of LAPACK's DSYTF2/DSYTRS path, adapted from
 * FracESSA's fast candidate filter. Copyright (c) 1992-2023 The University of
 * Tennessee and The University of Tennessee Research Foundation. Copyright (c)
 * 2000-2023 The University of California Berkeley. Copyright (c) 2006-2023 The
 * University of Colorado Denver. Redistribution and use in source and binary
 * forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * - Neither the name of the copyright holders nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
void swap_active_coordinates(double_matrix &system, size_t active_start,
                             size_t first, size_t second, size_t dimension,
                             size_t pivot_size) {
  if (first == second)
    return;
  for (size_t row = second + 1; row < dimension; ++row)
    std::swap(system(row, first), system(row, second));
  for (size_t row = first + 1; row < second; ++row)
    std::swap(system(row, first), system(second, row));
  std::swap(system(first, first), system(second, second));
  if (pivot_size == 2)
    std::swap(system(active_start + 1, active_start),
              system(second, active_start));
}

bool factor_and_forward_solve(double_matrix &system, size_t dimension,
                              std::vector<int> &pivots,
                              std::vector<double> &solution) {
  size_t k = 0;
  while (k < dimension) {
    size_t pivot_size = 1;
    size_t pivot_index = k;
    const double diagonal_magnitude = std::abs(system(k, k));
    size_t column_max_index = k;
    double column_maximum = 0.0;
    for (size_t row = k + 1; row < dimension; ++row) {
      const double magnitude = std::abs(system(row, k));
      if (magnitude > column_maximum) {
        column_maximum = magnitude;
        column_max_index = row;
      }
    }
    const double active_maximum = std::max(diagonal_magnitude, column_maximum);
    if (!std::isfinite(active_maximum) ||
        active_maximum < floating_pivot_cutoff)
      return false;
    if (diagonal_magnitude < bunch_kaufman_alpha * column_maximum) {
      double row_maximum = 0.0;
      for (size_t column = k; column < column_max_index; ++column)
        row_maximum =
            std::max(row_maximum, std::abs(system(column_max_index, column)));
      for (size_t row = column_max_index + 1; row < dimension; ++row)
        row_maximum =
            std::max(row_maximum, std::abs(system(row, column_max_index)));
      if (!std::isfinite(row_maximum) || row_maximum == 0.0)
        return false;
      if (diagonal_magnitude >= bunch_kaufman_alpha * column_maximum *
                                    (column_maximum / row_maximum)) {
        pivot_index = k;
      } else if (std::abs(system(column_max_index, column_max_index)) >=
                 bunch_kaufman_alpha * row_maximum) {
        pivot_index = column_max_index;
      } else {
        pivot_index = column_max_index;
        pivot_size = 2;
      }
    }

    const size_t block_last = k + pivot_size - 1;
    swap_active_coordinates(system, k, block_last, pivot_index, dimension,
                            pivot_size);
    if (pivot_index != block_last)
      std::swap(solution[block_last], solution[pivot_index]);
    if (pivot_size == 1) {
      const double pivot = system(k, k);
      if (!std::isfinite(pivot) || std::abs(pivot) < floating_pivot_cutoff)
        return false;
      const double inverse_pivot = 1.0 / pivot;
      for (size_t column = k + 1; column < dimension; ++column) {
        const double multiplier = system(column, k) * inverse_pivot;
        for (size_t row = column; row < dimension; ++row)
          system(row, column) -= system(row, k) * multiplier;
      }
      for (size_t row = k + 1; row < dimension; ++row) {
        system(row, k) *= inverse_pivot;
        solution[row] -= system(row, k) * solution[k];
      }
      solution[k] /= pivot;
      pivots[k] = static_cast<int>(pivot_index + 1);
    } else {
      const double off_diagonal = system(k + 1, k);
      if (!std::isfinite(off_diagonal) ||
          std::abs(off_diagonal) < floating_pivot_cutoff)
        return false;
      const double lower_diagonal = system(k + 1, k + 1) / off_diagonal;
      const double upper_diagonal = system(k, k) / off_diagonal;
      const double determinant_factor = lower_diagonal * upper_diagonal - 1.0;
      if (!std::isfinite(determinant_factor) ||
          std::abs(determinant_factor) < floating_pivot_cutoff)
        return false;
      const double inverse_block_scale =
          1.0 / (determinant_factor * off_diagonal);
      for (size_t column = k + 2; column < dimension; ++column) {
        const double first_multiplier =
            inverse_block_scale *
            (lower_diagonal * system(column, k) - system(column, k + 1));
        const double second_multiplier =
            inverse_block_scale *
            (upper_diagonal * system(column, k + 1) - system(column, k));
        for (size_t row = column; row < dimension; ++row)
          system(row, column) -= system(row, k) * first_multiplier +
                                 system(row, k + 1) * second_multiplier;
        system(column, k) = first_multiplier;
        system(column, k + 1) = second_multiplier;
        solution[column] -= first_multiplier * solution[k] +
                            second_multiplier * solution[k + 1];
      }
      const double first_rhs = solution[k] / off_diagonal;
      const double second_rhs = solution[k + 1] / off_diagonal;
      solution[k] =
          (lower_diagonal * first_rhs - second_rhs) / determinant_factor;
      solution[k + 1] =
          (upper_diagonal * second_rhs - first_rhs) / determinant_factor;
      pivots[k] = pivots[k + 1] = -static_cast<int>(pivot_index + 1);
    }
    k += pivot_size;
  }
  return true;
}

bool solve_backward(const double_matrix &system, size_t dimension,
                    const std::vector<int> &pivots,
                    std::vector<double> &solution) {
  size_t k = dimension;
  while (k > 0) {
    const size_t block_last = k - 1;
    if (pivots[block_last] > 0) {
      for (size_t row = block_last + 1; row < dimension; ++row)
        solution[block_last] -= system(row, block_last) * solution[row];
      const size_t pivot_index = static_cast<size_t>(pivots[block_last] - 1);
      if (pivot_index != block_last)
        std::swap(solution[block_last], solution[pivot_index]);
      --k;
    } else {
      const size_t block_first = block_last - 1;
      for (size_t row = block_last + 1; row < dimension; ++row) {
        solution[block_last] -= system(row, block_last) * solution[row];
        solution[block_first] -= system(row, block_first) * solution[row];
      }
      const size_t pivot_index = static_cast<size_t>(-pivots[block_last] - 1);
      if (pivot_index != block_last)
        std::swap(solution[block_last], solution[pivot_index]);
      k -= 2;
    }
  }
  return std::all_of(solution.begin(), solution.end(),
                     [](double value) { return std::isfinite(value); });
}

struct floating_walk_face {
  enum class factorization_outcome { singleton, solved, inconclusive };

  bool terminal_candidate = false;
  double tolerance = 0.0;
  bool inconclusive = false;
  bool negative_witness_candidate = false;
  bool feasible_candidate = false;
  factorization_outcome factorization = factorization_outcome::inconclusive;
};

struct exact_walk_face {
  bool nonsingular = true;
  bool consistent = true;
  bool feasible = false;
  bool is_kkt = false;
  bool reduced_positive_definite = true;
  bool reduced_has_negative_eigenvalue = false;
  size_t rank = 0;
  size_t nullity = 0;
  size_t positive_inertia = 0;
  size_t negative_inertia = 0;
};

struct walk_choice_trace {
  std::string_view move = "none";
  size_t candidates = 0;
  size_t rejected_empty = 0;
  size_t rejected_path = 0;
  size_t rejected_covered = 0;
  std::vector<size_t> jitter_draws;
};

struct terminal_principal_trace {
  bool floating_candidate = false;
  bool exact_checked = false;
  bool nonsingular = false;
  bool positive_definite = false;
  size_t rank = 0;
  size_t positive_inertia = 0;
  size_t negative_inertia = 0;
};

enum class walk_closure_direction { upward, downward };

struct buffered_walk_closure {
  std::vector<size_t> indices;
  walk_closure_direction direction;
  std::string_view kind;
  bool floating_checked;
};

enum class seed_certificate_type { dickinson, upward, downward };

struct buffered_seed_certificate {
  support lower;
  support upper;
  seed_certificate_type type;
  std::string_view kind;
  size_t width;
  size_t upper_size;
};

#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
size_t last_optimized_certificate_count = 0;
size_t last_combined_ray_sweep_count = 0;
size_t last_combined_ray_improvement_count = 0;
size_t last_fixed_support_upper_size = 0;
size_t last_pair_curvature_exclusion_count = 0;
size_t last_support_curvature_exclusion_count = 0;
size_t last_downward_count = 0;
size_t last_high_float_rejection_count = 0;
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
      : dimension_(dimension), factorization_(dimension),
        floating_filter_(dimension), product_(dimension),
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
      : dimension_(dimension), factorization_(dimension),
        floating_filter_(dimension), product_(dimension),
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
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
    optimized_certificate_count_ = 0;
    combined_ray_sweep_count_ = 0;
    combined_ray_improvement_count_ = 0;
    pair_curvature_exclusion_count_ = 0;
    support_curvature_exclusion_count_ = 0;
    downward_count_ = 0;
    high_float_rejection_count_ = 0;
#endif
    supports_.emplace(matrix.rows());
    floating_filter_.prepare(matrix);
    prepare_floating_matrix(matrix);
    random_.seed(matrix_seed(matrix));
    walk_gap_ = std::max<size_t>(1, matrix.rows());
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

#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
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

  size_t no_hiding_actions_for_testing(const matrix_integer &matrix,
                                       const std::vector<size_t> &indices) {
    indices_ = indices;
    const exact_walk_face face = analyze_exact_walk(matrix);
    size_t actions = 0;
    if (face.reduced_has_negative_eigenvalue)
      actions |= 1;
    if (face.reduced_positive_definite && face.consistent && face.feasible &&
        walk_payoff_.sign() >= 0)
      actions |= 2;
    principal_.resize(indices.size(), indices.size());
    copy_principal(matrix, indices, principal_);
    if (factorization_.factorize_inplace(principal_) != 0 &&
        factorization_.is_positive_definite())
      actions |= 4;
    return actions;
  }

  bool kkt_walk_for_testing(const matrix_integer &matrix,
                            const std::vector<size_t> &seed) {
    supports_.emplace(matrix.rows());
    floating_filter_.prepare(matrix);
    prepare_floating_matrix(matrix);
    random_.seed(matrix_seed(matrix));
    return process_kkt_walk(matrix, seed);
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

  static std::string_view yes_no(bool value) noexcept {
    return value ? "yes" : "no";
  }

  static std::string_view sign_name(int sign) noexcept {
    return sign < 0 ? "negative" : sign > 0 ? "positive" : "zero";
  }

  static std::string_view floating_factorization_name(
      floating_walk_face::factorization_outcome outcome) noexcept {
    switch (outcome) {
    case floating_walk_face::factorization_outcome::singleton:
      return "singleton";
    case floating_walk_face::factorization_outcome::solved:
      return "solved";
    case floating_walk_face::factorization_outcome::inconclusive:
      return "inconclusive";
    }
    return "inconclusive";
  }

  static std::string_view
  curvature_name(floating_positive_semidefinite_filter::reduced_curvature
                     curvature) noexcept {
    switch (curvature) {
    case floating_positive_semidefinite_filter::reduced_curvature::
        positive_definite:
      return "positive_definite";
    case floating_positive_semidefinite_filter::reduced_curvature::
        positive_semidefinite:
      return "positive_semidefinite";
    case floating_positive_semidefinite_filter::reduced_curvature::other:
      return "other";
    }
    return "other";
  }

  static void append_numbers(std::ostringstream &event,
                             const std::vector<size_t> &numbers) {
    event << '[';
    for (size_t position = 0; position < numbers.size(); ++position) {
      if (position != 0)
        event << ',';
      event << numbers[position];
    }
    event << ']';
  }

  void record_walk_step(
      size_t step, const floating_walk_face *floating,
      const floating_positive_semidefinite_filter::reduced_curvature *curvature,
      const exact_walk_face *exact, std::string_view outcome,
      const std::optional<std::vector<size_t>> &next) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=improved_nbc_b9 n=" << dimension_
          << " walk=" << active_walk_id_ << " step=" << step
          << " frontier=" << certificate_frontier_ << " seed=";
    append_indices(event, active_walk_seed_);
    event << " support=";
    append_indices(event, indices_);
    event << " arithmetic="
          << (floating == nullptr ? "exact"
              : exact == nullptr  ? "floating"
                                  : "floating_exact")
          << " floating_factorization="
          << (floating == nullptr
                  ? "not_run"
                  : floating_factorization_name(floating->factorization))
          << " floating_feasible="
          << (floating == nullptr ? "not_run"
                                  : yes_no(floating->feasible_candidate))
          << " floating_terminal="
          << (floating == nullptr ? "not_run"
                                  : yes_no(floating->terminal_candidate))
          << " floating_negative_candidate="
          << (floating == nullptr
                  ? "not_run"
                  : yes_no(floating->negative_witness_candidate))
          << " curvature_filter="
          << (curvature == nullptr ? "not_run" : curvature_name(*curvature));
    if (exact == nullptr) {
      event << " exact_factorization=not_run exact_rank=na exact_nullity=na"
               " exact_positive_inertia=na exact_negative_inertia=na"
               " exact_consistent=not_run exact_feasible=not_run "
               "exact_kkt=not_run"
               " exact_reduced_pd=not_run exact_reduced_negative=not_run "
               "exact_payoff_sign=na";
    } else {
      event << " exact_factorization="
            << (indices_.size() == 1 ? "singleton"
                : exact->nonsingular ? "nonsingular"
                                     : "singular")
            << " exact_rank=" << exact->rank
            << " exact_nullity=" << exact->nullity
            << " exact_positive_inertia=" << exact->positive_inertia
            << " exact_negative_inertia=" << exact->negative_inertia
            << " exact_consistent=" << yes_no(exact->consistent)
            << " exact_feasible=" << yes_no(exact->feasible)
            << " exact_kkt=" << yes_no(exact->is_kkt)
            << " exact_reduced_pd=" << yes_no(exact->reduced_positive_definite)
            << " exact_reduced_negative="
            << yes_no(exact->reduced_has_negative_eigenvalue)
            << " exact_payoff_sign="
            << (exact->consistent && exact->feasible
                    ? sign_name(walk_payoff_.sign())
                    : std::string_view{"na"});
    }
    event << " move=" << walk_choice_trace_.move
          << " candidates=" << walk_choice_trace_.candidates
          << " jitter_draws=";
    append_numbers(event, walk_choice_trace_.jitter_draws);
    event << " rejected_empty=" << walk_choice_trace_.rejected_empty
          << " rejected_path=" << walk_choice_trace_.rejected_path
          << " rejected_covered=" << walk_choice_trace_.rejected_covered
          << " next=";
    if (next)
      append_indices(event, *next);
    else
      event << "none";
    event << " buffered_closures=" << buffered_walk_closures_.size()
          << " outcome=" << outcome;
    diagnostics::record_history_event("heuristic_walk_step", event.str());
  }

  void record_terminal_principal_factorization(
      size_t step, const terminal_principal_trace &trace) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=improved_nbc_b9 n=" << dimension_
          << " walk=" << active_walk_id_ << " step=" << step
          << " frontier=" << certificate_frontier_ << " seed=";
    append_indices(event, active_walk_seed_);
    event << " support=";
    append_indices(event, indices_);
    event << " matrix=principal floating_candidate="
          << yes_no(trace.floating_candidate)
          << " exact_checked=" << yes_no(trace.exact_checked)
          << " exact_factorization="
          << (!trace.exact_checked ? "not_run"
              : trace.nonsingular  ? "nonsingular"
                                   : "singular")
          << " exact_rank=";
    if (trace.exact_checked)
      event << trace.rank;
    else
      event << "na";
    event << " exact_nullity=";
    if (trace.exact_checked)
      event << indices_.size() - trace.rank;
    else
      event << "na";
    event << " exact_positive_inertia=";
    if (trace.exact_checked)
      event << trace.positive_inertia;
    else
      event << "na";
    event << " exact_negative_inertia=";
    if (trace.exact_checked)
      event << trace.negative_inertia;
    else
      event << "na";
    event << " exact_positive_definite="
          << (trace.exact_checked ? yes_no(trace.positive_definite)
                                  : std::string_view{"not_run"})
          << " buffered_closures=" << buffered_walk_closures_.size();
    diagnostics::record_history_event("heuristic_walk_terminal_factorization",
                                      event.str());
  }

  void record_interval_certificate(const support &lower, const support &upper) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=improved_nbc_b9 n=" << product_.size()
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
                                  const std::vector<size_t> &source,
                                  bool floating_checked) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=improved_nbc_b9 n=" << product_.size()
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
    event << " floating_checked=" << (floating_checked ? "yes" : "no")
          << " exact_checked=yes";
    diagnostics::record_history_event("certificate", event.str());
  }

  void record_high_support_without_certificate(bool exact_checked) {
    if (!diagnostics_.active())
      return;
    std::ostringstream event;
    event << "model=improved_nbc_b9 n=" << product_.size()
          << " frontier=high source=";
    append_indices(event, indices_);
    event << " floating_checked=yes exact_checked="
          << (exact_checked ? "yes" : "no");
    diagnostics::record_history_event("visited_support", event.str());
  }

  void record_pair_curvature_certificate(size_t first, size_t second) {
    record_closure_certificate("pair_curvature", "upward", "initial",
                               {first, second}, false);
  }

  bool process_low_support(const matrix_integer &matrix, size_t &low,
                           size_t high) {
    while (low <= high) {
      diagnostics_.stage(low);
      COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("stage_low", low);
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
      COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("stage_high", high);
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
    COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS(
        from_low_frontier ? "process_low" : "process_high", indices_.size());
    const bool ran_walk = walk_due(from_low_frontier);
    if (ran_walk) {
      const std::vector<size_t> seed = indices_;
      defer_seed_certificate_ = true;
      deferred_seed_certificate_.reset();
      const bool seed_result =
          process_subset(matrix, direction, from_low_frontier);
      defer_seed_certificate_ = false;
      if (!seed_result)
        return false;

      const bool walk_result = process_kkt_walk(matrix, seed);
      complete_walk(from_low_frontier);
      indices_ = seed;
      if (!walk_result)
        return false;
      commit_deferred_seed_certificate();
    } else if (!process_subset(matrix, direction, from_low_frontier)) {
      return false;
    }

    if (!ran_walk && initial_low_walk_done_ && initial_high_walk_done_ &&
        supports_since_walk_ != std::numeric_limits<size_t>::max())
      ++supports_since_walk_;
    return true;
  }

  bool walk_due(bool from_low_frontier) const noexcept {
    if (from_low_frontier && !initial_low_walk_done_)
      return true;
    if (!from_low_frontier && !initial_high_walk_done_)
      return true;
    return initial_low_walk_done_ && initial_high_walk_done_ &&
           supports_since_walk_ >= walk_gap_ &&
           next_walk_from_low_ == from_low_frontier;
  }

  void complete_walk(bool from_low_frontier) noexcept {
    const bool initial =
        from_low_frontier ? !initial_low_walk_done_ : !initial_high_walk_done_;
    if (from_low_frontier)
      initial_low_walk_done_ = true;
    else
      initial_high_walk_done_ = true;
    supports_since_walk_ = 0;
    if (initial)
      return;
    next_walk_from_low_ = !next_walk_from_low_;
    walk_gap_ = next_walk_gap(dimension_, walk_gap_, last_walk_reached_kkt_);
  }

  static uint64_t matrix_seed(const matrix_integer &matrix) noexcept {
    uint64_t hash = 1469598103934665603ULL;
    for (size_t row = 0; row < matrix.rows(); ++row) {
      for (size_t column = 0; column <= row; ++column) {
        const auto value = matrix(row, column);
        const uint64_t residue = static_cast<uint64_t>(
            fmpz_fdiv_ui(value.native_handle(), 4'294'967'291UL));
        hash ^= residue ^ (value.sign() < 0 ? 0x9e3779b97f4a7c15ULL : 0ULL);
        hash *= 1099511628211ULL;
      }
    }
    return hash ^ 0x696d70726f766564ULL;
  }

  void prepare_floating_matrix(const matrix_integer &matrix) {
    floating_matrix_.resize(dimension_, dimension_);
    integer maximum;
    bool initialized = false;
    for (size_t row = 0; row < dimension_; ++row) {
      for (size_t column = 0; column <= row; ++column) {
        const auto value = matrix(row, column);
        if (value.is_zero())
          continue;
        if (!initialized || value.compare_abs(maximum) > 0) {
          maximum.set_abs(value);
          initialized = true;
        }
      }
    }
    if (!initialized)
      return;

    slong maximum_exponent = 0;
    static_cast<void>(maximum.to_dbl_2exp(maximum_exponent));
    for (size_t row = 0; row < dimension_; ++row) {
      for (size_t column = 0; column <= row; ++column) {
        const auto value = matrix(row, column);
        if (value.is_zero())
          continue;
        slong exponent = 0;
        const double mantissa = value.to_dbl_2exp(exponent);
        const slong difference = exponent - maximum_exponent;
        const double converted =
            difference < std::numeric_limits<int>::min()
                ? 0.0
                : std::scalbn(mantissa, static_cast<int>(difference));
        if (!std::isfinite(converted))
          throw std::runtime_error(
              "Improved NBC-B9 floating matrix conversion is not finite");
        floating_matrix_(row, column) = converted;
        floating_matrix_(column, row) = converted;
      }
    }
  }

  floating_walk_face analyze_floating_walk() {
    const size_t cardinality = indices_.size();
    auto factorization = floating_walk_face::factorization_outcome::singleton;
    floating_solution_.assign(cardinality, 0.0);
    floating_products_.assign(dimension_, 0.0);
    if (cardinality == 1) {
      floating_solution_[0] = 1.0;
    } else {
      const size_t reduced_dimension = cardinality - 1;
      const size_t reference = indices_.back();
      floating_reduced_.resize(reduced_dimension, reduced_dimension);
      floating_rhs_.assign(reduced_dimension, 0.0);
      floating_pivots_.assign(reduced_dimension, 0);
      double maximum = 0.0;
      for (size_t row = 0; row < reduced_dimension; ++row) {
        const size_t original_row = indices_[row];
        floating_rhs_[row] = floating_matrix_(reference, reference) -
                             floating_matrix_(original_row, reference);
        for (size_t column = 0; column <= row; ++column) {
          const size_t original_column = indices_[column];
          const double value = floating_matrix_(original_row, original_column) -
                               floating_matrix_(original_row, reference) -
                               floating_matrix_(reference, original_column) +
                               floating_matrix_(reference, reference);
          floating_reduced_(row, column) = value;
          maximum = std::max(maximum, std::abs(value));
        }
      }
      if (!(maximum > 0.0) || !std::isfinite(maximum))
        return {false, 0.0,
                true,  false,
                false, floating_walk_face::factorization_outcome::inconclusive};
      const double inverse_scale = 1.0 / maximum;
      for (size_t row = 0; row < reduced_dimension; ++row) {
        floating_rhs_[row] *= inverse_scale;
        for (size_t column = 0; column <= row; ++column)
          floating_reduced_(row, column) *= inverse_scale;
      }
      if (!factor_and_forward_solve(floating_reduced_, reduced_dimension,
                                    floating_pivots_, floating_rhs_) ||
          !solve_backward(floating_reduced_, reduced_dimension,
                          floating_pivots_, floating_rhs_))
        return {false, 0.0,
                true,  false,
                false, floating_walk_face::factorization_outcome::inconclusive};
      factorization = floating_walk_face::factorization_outcome::solved;

      double sum = 0.0;
      for (size_t row = 0; row < reduced_dimension; ++row) {
        floating_solution_[row] = floating_rhs_[row];
        sum += floating_rhs_[row];
      }
      floating_solution_.back() = 1.0 - sum;
    }

    for (size_t row = 0; row < dimension_; ++row) {
      for (size_t position = 0; position < cardinality; ++position)
        floating_products_[row] += floating_matrix_(row, indices_[position]) *
                                   floating_solution_[position];
      if (!std::isfinite(floating_products_[row]))
        return {false, 0.0, true, false, false, factorization};
    }
    floating_payoff_ = floating_products_[indices_.back()];
    double scale = std::max(1.0, std::abs(floating_payoff_));
    for (double value : floating_solution_)
      scale = std::max(scale, std::abs(value));
    for (double value : floating_products_)
      scale = std::max(scale, std::abs(value));
    const double tolerance = 256.0 * std::numeric_limits<double>::epsilon() *
                             static_cast<double>(dimension_ + 1) * scale;
    bool feasible = true;
    bool has_zero = false;
    for (double value : floating_solution_) {
      feasible &= value >= -tolerance;
      has_zero |= std::abs(value) <= tolerance;
    }
    const bool negative = feasible && floating_payoff_ < -tolerance;
    if (!feasible || has_zero)
      return {false, tolerance, false, negative, feasible, factorization};
    for (size_t index = 0; index < dimension_; ++index) {
      if (std::binary_search(indices_.begin(), indices_.end(), index))
        continue;
      if (floating_products_[index] < floating_payoff_ - tolerance)
        return {false, tolerance, false, negative, true, factorization};
    }
    return {true, tolerance, false, negative, true, factorization};
  }

  support make_support(const std::vector<size_t> &indices) const {
    support result(dimension_);
    for (const size_t index : indices)
      result.set(index);
    return result;
  }

  size_t jittered_rank(size_t count) {
    size_t rank = 0;
    while (rank + 1 < count && random_() % 3 == 0)
      ++rank;
    return rank;
  }

  std::optional<std::vector<size_t>>
  choose_successor(std::vector<std::vector<size_t>> candidates,
                   std::string_view move) {
    const bool trace_choices = diagnostics_.active();
    if (trace_choices) {
      walk_choice_trace_ = {};
      walk_choice_trace_.move = move;
      walk_choice_trace_.candidates = candidates.size();
    }
    while (!candidates.empty()) {
      const size_t rank = jittered_rank(candidates.size());
      if (trace_choices)
        walk_choice_trace_.jitter_draws.push_back(rank);
      std::vector<size_t> candidate = std::move(candidates[rank]);
      candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(rank));
      if (candidate.empty()) {
        if (trace_choices)
          ++walk_choice_trace_.rejected_empty;
        continue;
      }
      const support candidate_support = make_support(candidate);
      if (path_visited_.find(candidate_support) != path_visited_.end()) {
        if (trace_choices)
          ++walk_choice_trace_.rejected_path;
        continue;
      }
      if (supports_->covers(candidate_support)) {
        if (trace_choices)
          ++walk_choice_trace_.rejected_covered;
        continue;
      }
      return candidate;
    }
    return std::nullopt;
  }

  std::optional<std::vector<size_t>>
  floating_walk_successor(const floating_walk_face &face) {
    std::vector<size_t> positions;
    for (size_t position = 0; position < indices_.size(); ++position)
      if (floating_solution_[position] < -face.tolerance)
        positions.push_back(position);
    std::sort(
        positions.begin(), positions.end(), [&](size_t left, size_t right) {
          return floating_solution_[left] != floating_solution_[right]
                     ? floating_solution_[left] < floating_solution_[right]
                     : indices_[left] < indices_[right];
        });
    if (!positions.empty()) {
      std::vector<std::vector<size_t>> candidates;
      for (const size_t removed : positions) {
        std::vector<size_t> next = indices_;
        next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
        candidates.push_back(std::move(next));
      }
      return choose_successor(std::move(candidates), "remove_negative");
    }

    std::vector<size_t> nonzero;
    std::vector<size_t> zeros;
    for (size_t position = 0; position < indices_.size(); ++position) {
      if (std::abs(floating_solution_[position]) <= face.tolerance)
        zeros.push_back(position);
      else
        nonzero.push_back(indices_[position]);
    }
    if (!zeros.empty()) {
      std::vector<std::vector<size_t>> candidates;
      candidates.push_back(nonzero);
      for (const size_t removed : zeros) {
        std::vector<size_t> next = indices_;
        next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
        if (next != nonzero)
          candidates.push_back(std::move(next));
      }
      return choose_successor(std::move(candidates), "remove_zero");
    }

    const support current = make_support(indices_);
    std::vector<size_t> additions;
    for (size_t index = 0; index < dimension_; ++index)
      if (!current.contains(index) &&
          floating_products_[index] < floating_payoff_ - face.tolerance)
        additions.push_back(index);
    std::sort(
        additions.begin(), additions.end(), [&](size_t left, size_t right) {
          return floating_products_[left] != floating_products_[right]
                     ? floating_products_[left] < floating_products_[right]
                     : left < right;
        });
    std::vector<std::vector<size_t>> candidates;
    for (const size_t added : additions) {
      std::vector<size_t> next = indices_;
      next.insert(std::lower_bound(next.begin(), next.end(), added), added);
      candidates.push_back(std::move(next));
    }
    return choose_successor(std::move(candidates), "add_violation");
  }

  exact_walk_face analyze_exact_walk(const matrix_integer &matrix) {
    const size_t cardinality = indices_.size();
    exact_walk_face face;
    walk_solution_.resize(cardinality, 1);
    walk_products_.resize(dimension_, 1);
    if (cardinality == 1) {
      walk_denominator_.set_one();
      walk_solution_(0, 0).set_one();
      walk_payoff_ = matrix(indices_[0], indices_[0]);
    } else {
      const size_t reduced_dimension = cardinality - 1;
      const size_t reference = indices_.back();
      walk_reduced_.resize(reduced_dimension, reduced_dimension);
      walk_rhs_.resize(reduced_dimension, 1);
      for (size_t row = 0; row < reduced_dimension; ++row) {
        const size_t original_row = indices_[row];
        walk_rhs_(row, 0).set_difference(matrix(reference, reference),
                                         matrix(original_row, reference));
        for (size_t column = 0; column <= row; ++column) {
          const size_t original_column = indices_[column];
          walk_reduced_(row, column) = matrix(original_row, original_column);
          walk_reduced_(row, column) -= matrix(original_row, reference);
          walk_reduced_(row, column) -= matrix(reference, original_column);
          walk_reduced_(row, column) += matrix(reference, reference);
        }
      }
      face.nonsingular = factorization_.factorize_inplace(walk_reduced_) != 0;
      face.rank = factorization_.rank();
      face.nullity = reduced_dimension - factorization_.rank();
      face.negative_inertia = factorization_.negative_inertia();
      face.positive_inertia = face.rank - face.negative_inertia;
      face.reduced_positive_definite =
          face.nonsingular && factorization_.is_positive_definite();
      face.reduced_has_negative_eigenvalue = face.negative_inertia != 0;
      if (face.nonsingular) {
        factorization_.solve_inplace(walk_rhs_, walk_denominator_,
                                     walk_reduced_);
      } else {
        walk_original_rhs_ = walk_rhs_;
        face.consistent = factorization_.solve_consistent_inplace(
            walk_rhs_, walk_denominator_, walk_reduced_);
        if (!face.consistent)
          return face;
      }
      assert(walk_denominator_.sign() > 0);

      integer sum;
      for (size_t row = 0; row < reduced_dimension; ++row) {
        walk_solution_(row, 0) = walk_rhs_(row, 0);
        sum += walk_rhs_(row, 0);
      }
      walk_solution_(cardinality - 1, 0).set_difference(walk_denominator_, sum);
      walk_payoff_.set_zero();
      for (size_t position = 0; position < cardinality; ++position)
        walk_payoff_.addmul(matrix(reference, indices_[position]),
                            walk_solution_(position, 0));
    }

    face.feasible = true;
    for (size_t position = 0; position < cardinality; ++position)
      face.feasible &= walk_solution_(position, 0).sign() >= 0;
    if (!face.feasible)
      return face;

    face.is_kkt = true;
    for (size_t row = 0; row < dimension_; ++row) {
      walk_products_(row, 0).set_zero();
      for (size_t position = 0; position < cardinality; ++position)
        walk_products_(row, 0).addmul(matrix(row, indices_[position]),
                                      walk_solution_(position, 0));
      if (walk_products_(row, 0).compare(walk_payoff_) < 0)
        face.is_kkt = false;
    }
    return face;
  }

  void buffer_walk_closure(walk_closure_direction direction,
                           std::string_view kind, bool floating_checked) {
    const auto dominates_new = [&](const buffered_walk_closure &closure) {
      if (closure.direction != direction)
        return false;
      return direction == walk_closure_direction::upward
                 ? std::includes(indices_.begin(), indices_.end(),
                                 closure.indices.begin(), closure.indices.end())
                 : std::includes(closure.indices.begin(), closure.indices.end(),
                                 indices_.begin(), indices_.end());
    };
    if (std::any_of(buffered_walk_closures_.begin(),
                    buffered_walk_closures_.end(), dominates_new))
      return;
    buffered_walk_closures_.erase(
        std::remove_if(
            buffered_walk_closures_.begin(), buffered_walk_closures_.end(),
            [&](const buffered_walk_closure &closure) {
              if (closure.direction != direction)
                return false;
              return direction == walk_closure_direction::upward
                         ? std::includes(closure.indices.begin(),
                                         closure.indices.end(),
                                         indices_.begin(), indices_.end())
                         : std::includes(indices_.begin(), indices_.end(),
                                         closure.indices.begin(),
                                         closure.indices.end());
            }),
        buffered_walk_closures_.end());
    buffered_walk_closures_.push_back(
        {indices_, direction, kind, floating_checked});
  }

  bool record_walk_zero_witness() {
    if (classification_ != nullptr) {
      classification_->is_strictly_copositive = false;
      return true;
    }
    return mode_ != copositivity_mode::strictly_copositive;
  }

  std::string exact_kkt_signature() const {
    integer divisor(walk_denominator_);
    if (divisor.sign() < 0)
      divisor.negate();
    for (size_t position = 0; position < indices_.size(); ++position) {
      if (walk_solution_(position, 0).sign() > 0)
        fmpz_gcd(divisor.native_handle(), divisor.native_handle(),
                 walk_solution_(position, 0).native_handle());
    }
    integer normalized_denominator(walk_denominator_);
    normalized_denominator.divide_exact(divisor);
    std::ostringstream signature;
    signature << normalized_denominator.to_string() << ':';
    for (size_t position = 0; position < indices_.size(); ++position) {
      if (walk_solution_(position, 0).sign() <= 0)
        continue;
      integer normalized(walk_solution_(position, 0));
      normalized.divide_exact(divisor);
      signature << indices_[position] << '=' << normalized.to_string() << ';';
    }
    return signature.str();
  }

  bool analyze_and_buffer_exact_walk_support(const matrix_integer &matrix,
                                             exact_walk_face &face,
                                             bool floating_checked) {
    face = analyze_exact_walk(matrix);
    if (face.reduced_has_negative_eigenvalue)
      buffer_walk_closure(walk_closure_direction::upward,
                          "walk_negative_curvature", floating_checked);
    if (face.consistent && face.feasible && walk_payoff_.sign() < 0)
      return false;
    if (face.consistent && face.feasible && walk_payoff_.is_zero() &&
        !record_walk_zero_witness())
      return false;
    if (face.reduced_positive_definite && face.consistent && face.feasible &&
        walk_payoff_.sign() >= 0)
      buffer_walk_closure(walk_closure_direction::downward,
                          "walk_strict_face_minimum", floating_checked);
    return true;
  }

  std::optional<std::vector<size_t>>
  exact_nonsingular_walk_successor(const exact_walk_face &face) {
    std::vector<size_t> positions;
    for (size_t position = 0; position < indices_.size(); ++position)
      if (walk_solution_(position, 0).sign() < 0)
        positions.push_back(position);
    std::sort(positions.begin(), positions.end(),
              [&](size_t left, size_t right) {
                const int comparison =
                    walk_solution_(left, 0).compare(walk_solution_(right, 0));
                return comparison != 0 ? comparison < 0
                                       : indices_[left] < indices_[right];
              });
    if (!positions.empty()) {
      std::vector<std::vector<size_t>> candidates;
      for (const size_t removed : positions) {
        std::vector<size_t> next = indices_;
        next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
        candidates.push_back(std::move(next));
      }
      return choose_successor(std::move(candidates), "remove_negative");
    }

    std::vector<size_t> nonzero;
    std::vector<size_t> zeros;
    for (size_t position = 0; position < indices_.size(); ++position) {
      if (walk_solution_(position, 0).is_zero())
        zeros.push_back(position);
      else
        nonzero.push_back(indices_[position]);
    }
    if (!zeros.empty()) {
      std::vector<std::vector<size_t>> candidates;
      candidates.push_back(nonzero);
      for (const size_t removed : zeros) {
        std::vector<size_t> next = indices_;
        next.erase(next.begin() + static_cast<std::ptrdiff_t>(removed));
        if (next != nonzero)
          candidates.push_back(std::move(next));
      }
      return choose_successor(std::move(candidates), "remove_zero");
    }
    if (face.is_kkt)
      return std::nullopt;

    const support current = make_support(indices_);
    std::vector<size_t> additions;
    for (size_t index = 0; index < dimension_; ++index)
      if (!current.contains(index) &&
          walk_products_(index, 0).compare(walk_payoff_) < 0)
        additions.push_back(index);
    std::sort(additions.begin(), additions.end(),
              [&](size_t left, size_t right) {
                const int comparison =
                    walk_products_(left, 0).compare(walk_products_(right, 0));
                return comparison != 0 ? comparison < 0 : left < right;
              });
    std::vector<std::vector<size_t>> candidates;
    for (const size_t added : additions) {
      std::vector<size_t> next = indices_;
      next.insert(std::lower_bound(next.begin(), next.end(), added), added);
      candidates.push_back(std::move(next));
    }
    return choose_successor(std::move(candidates), "add_violation");
  }

  std::optional<std::vector<size_t>> boundary_walk_support(size_t column,
                                                           int orientation) {
    walk_direction_.assign(indices_.size(), integer{});
    for (size_t row = 0; row + 1 < indices_.size(); ++row) {
      walk_direction_[row] = walk_basis_(row, column);
      if (orientation < 0)
        walk_direction_[row].negate();
      walk_direction_.back() -= walk_direction_[row];
    }
    std::optional<size_t> minimum;
    for (size_t position = 0; position < walk_direction_.size(); ++position) {
      if (walk_direction_[position].sign() >= 0)
        continue;
      if (!minimum ||
          walk_direction_[position].compare(walk_direction_[*minimum]) < 0)
        minimum = position;
    }
    if (!minimum)
      return std::nullopt;
    std::vector<size_t> next;
    for (size_t position = 0; position < indices_.size(); ++position)
      if (walk_direction_[position].compare(walk_direction_[*minimum]) != 0)
        next.push_back(indices_[position]);
    return next;
  }

  std::optional<std::vector<size_t>>
  exact_singular_walk_successor(const exact_walk_face &face) {
    const size_t reduced_dimension = indices_.size() - 1;
    walk_basis_.resize(reduced_dimension, face.nullity);
    factorization_.nullspace_basis(walk_basis_, walk_reduced_);
    std::vector<std::vector<size_t>> candidates;
    if (face.consistent) {
      for (size_t column = 0; column < face.nullity; ++column) {
        if (auto next = boundary_walk_support(column, 1))
          candidates.push_back(std::move(*next));
        if (auto next = boundary_walk_support(column, -1))
          candidates.push_back(std::move(*next));
      }
      return choose_successor(std::move(candidates), "singular_nullspace");
    }

    bool found_direction = false;
    for (size_t column = 0; column < face.nullity; ++column) {
      integer dot;
      for (size_t row = 0; row < reduced_dimension; ++row)
        dot.addmul(walk_basis_(row, column), walk_original_rhs_(row, 0));
      if (dot.is_zero())
        continue;
      found_direction = true;
      if (auto next = boundary_walk_support(column, dot.sign() > 0 ? 1 : -1))
        candidates.push_back(std::move(*next));
    }
    if (!found_direction)
      throw std::logic_error("an inconsistent symmetric KKT system has no "
                             "separating nullspace direction");
    return choose_successor(std::move(candidates), "singular_separation");
  }

  bool screen_walk_curvature(
      const floating_walk_face &floating, const matrix_integer &matrix,
      bool &exact_known, exact_walk_face &exact,
      floating_positive_semidefinite_filter::reduced_curvature &curvature) {
    curvature = floating_filter_.classify_reduced_hessian(indices_);
    const bool downward_candidate =
        curvature == floating_positive_semidefinite_filter::reduced_curvature::
                         positive_definite &&
        floating.feasible_candidate && floating_payoff_ >= -floating.tolerance;
    if (curvature !=
            floating_positive_semidefinite_filter::reduced_curvature::other &&
        !downward_candidate)
      return true;
    if (!exact_known) {
      const bool continue_search =
          analyze_and_buffer_exact_walk_support(matrix, exact, true);
      exact_known = true;
      if (!continue_search)
        return false;
    }
    return true;
  }

  terminal_principal_trace
  verify_terminal_principal_pd(const matrix_integer &matrix) {
    terminal_principal_trace trace;
    for (buffered_walk_closure &closure : buffered_walk_closures_)
      if (closure.indices == indices_)
        closure.floating_checked = true;
    if (!floating_filter_.looks_positive_semidefinite(indices_))
      return trace;
    trace.floating_candidate = true;
    principal_.resize(indices_.size(), indices_.size());
    copy_principal(matrix, indices_, principal_);
    trace.nonsingular = factorization_.factorize_inplace(principal_) != 0;
    trace.exact_checked = true;
    trace.rank = factorization_.rank();
    trace.negative_inertia = factorization_.negative_inertia();
    trace.positive_inertia = trace.rank - trace.negative_inertia;
    trace.positive_definite = factorization_.is_positive_definite();
    if (trace.nonsingular && trace.positive_definite)
      buffer_walk_closure(walk_closure_direction::downward,
                          "walk_positive_definite_block", true);
    return trace;
  }

  void commit_walk_closures() {
    support floor(dimension_);
    support ceiling(dimension_);
    ceiling.set_all();
    for (const buffered_walk_closure &closure : buffered_walk_closures_) {
      const support root = make_support(closure.indices);
      const support &lower =
          closure.direction == walk_closure_direction::upward ? root : floor;
      const support &upper =
          closure.direction == walk_closure_direction::upward ? ceiling : root;
      if (supports_->covers_interval(lower, upper))
        continue;
      supports_->add_interval(lower, upper);
      if (closure.direction == walk_closure_direction::upward) {
        diagnostics_.certificate(dimension_ - closure.indices.size(),
                                 dimension_);
        record_closure_certificate(closure.kind, "upward", "walk",
                                   closure.indices, closure.floating_checked);
        COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("walk_upward",
                                            closure.indices.size());
      } else {
        diagnostics_.certificate();
        record_closure_certificate(closure.kind, "downward", "walk",
                                   closure.indices, closure.floating_checked);
        COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("walk_downward",
                                            closure.indices.size());
      }
    }
  }

  bool commit_deferred_seed_certificate() {
    if (!deferred_seed_certificate_)
      return false;
    buffered_seed_certificate certificate =
        std::move(*deferred_seed_certificate_);
    deferred_seed_certificate_.reset();
    if (supports_->covers_interval(certificate.lower, certificate.upper))
      return false;

    supports_->add_interval(certificate.lower, certificate.upper);
    switch (certificate.type) {
    case seed_certificate_type::dickinson:
      if (diagnostics_.active())
        diagnostics_.certificate(certificate.width, certificate.upper_size);
      record_interval_certificate(certificate.lower, certificate.upper);
      COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("dickinson", indices_.size());
      break;
    case seed_certificate_type::upward:
      if (diagnostics_.active())
        diagnostics_.certificate(product_.size() - indices_.size(),
                                 product_.size());
      record_closure_certificate("support_curvature", "upward",
                                 certificate_frontier_, indices_, false);
      COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("support_upward", indices_.size());
      break;
    case seed_certificate_type::downward:
      diagnostics_.certificate();
      record_closure_certificate(certificate.kind, "downward",
                                 certificate_frontier_, indices_,
                                 certificate_frontier_ == "high");
      COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("downward", indices_.size());
      break;
    }
    return true;
  }

  bool process_kkt_walk(const matrix_integer &matrix,
                        const std::vector<size_t> &seed) {
    if (seed.empty() || supports_->covers(make_support(seed)))
      return true;
    active_walk_id_ = ++walk_count_;
    active_walk_seed_ = seed;
    indices_ = seed;
    path_visited_.clear();
    buffered_walk_closures_.clear();
    walk_reached_kkt_ = false;
    path_visited_.insert(make_support(seed));
    bool exact_mode = false;
    size_t visited_steps = 0;
    for (size_t visited = 0; visited < dimension_; ++visited) {
      timeout_checkpoint();
      visited_steps = visited + 1;
      if (diagnostics_.active())
        walk_choice_trace_ = {};
      std::optional<std::vector<size_t>> next;
      if (exact_mode) {
        exact_walk_face exact;
        if (!analyze_and_buffer_exact_walk_support(matrix, exact, false)) {
          record_walk_step(visited_steps, nullptr, nullptr, &exact,
                           walk_payoff_.sign() < 0 ? "negative_witness"
                                                   : "zero_witness",
                           next);
          return false;
        }
        if (exact.consistent && exact.feasible && exact.is_kkt) {
          walk_reached_kkt_ = true;
          const bool novel =
              kkt_signatures_.insert(exact_kkt_signature()).second;
          COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS(
              novel ? "walk_kkt_new" : "walk_kkt_duplicate", indices_.size());
          record_walk_step(visited_steps, nullptr, nullptr, &exact,
                           novel ? "kkt_new" : "kkt_duplicate", next);
          break;
        }
        next = exact.nonsingular ? exact_nonsingular_walk_successor(exact)
                                 : exact_singular_walk_successor(exact);
        record_walk_step(visited_steps, nullptr, nullptr, &exact,
                         !next                         ? "no_successor"
                         : visited_steps == dimension_ ? "step_limit"
                                                       : "continue_exact",
                         next);
      } else {
        const floating_walk_face floating = analyze_floating_walk();
        if (floating.inconclusive) {
          record_walk_step(visited_steps, &floating, nullptr, nullptr,
                           "floating_inconclusive", next);
          break;
        }
        exact_walk_face exact;
        bool exact_known = false;
        auto curvature =
            floating_positive_semidefinite_filter::reduced_curvature::other;
        if (!screen_walk_curvature(floating, matrix, exact_known, exact,
                                   curvature)) {
          record_walk_step(visited_steps, &floating, &curvature, &exact,
                           walk_payoff_.sign() < 0 ? "negative_witness"
                                                   : "zero_witness",
                           next);
          return false;
        }
        if (floating.negative_witness_candidate) {
          if (!exact_known) {
            const bool continue_search =
                analyze_and_buffer_exact_walk_support(matrix, exact, true);
            exact_known = true;
            if (!continue_search) {
              record_walk_step(visited_steps, &floating, &curvature, &exact,
                               walk_payoff_.sign() < 0 ? "negative_witness"
                                                       : "zero_witness",
                               next);
              return false;
            }
          }
        }
        if (floating.terminal_candidate) {
          if (!exact_known) {
            const bool continue_search =
                analyze_and_buffer_exact_walk_support(matrix, exact, true);
            exact_known = true;
            if (!continue_search) {
              record_walk_step(visited_steps, &floating, &curvature, &exact,
                               walk_payoff_.sign() < 0 ? "negative_witness"
                                                       : "zero_witness",
                               next);
              return false;
            }
          }
          if (exact.consistent && exact.feasible && exact.is_kkt) {
            walk_reached_kkt_ = true;
            const bool novel =
                kkt_signatures_.insert(exact_kkt_signature()).second;
            COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS(
                novel ? "walk_kkt_new" : "walk_kkt_duplicate", indices_.size());
            record_walk_step(visited_steps, &floating, &curvature, &exact,
                             novel ? "kkt_new" : "kkt_duplicate", next);
            break;
          }
          exact_mode = true;
          COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("walk_exact_continuation",
                                              indices_.size());
          next = exact.nonsingular ? exact_nonsingular_walk_successor(exact)
                                   : exact_singular_walk_successor(exact);
          record_walk_step(visited_steps, &floating, &curvature, &exact,
                           !next                         ? "no_successor"
                           : visited_steps == dimension_ ? "step_limit"
                                                         : "exact_continuation",
                           next);
        } else {
          next = floating_walk_successor(floating);
          record_walk_step(visited_steps, &floating, &curvature,
                           exact_known ? &exact : nullptr,
                           !next                         ? "no_successor"
                           : visited_steps == dimension_ ? "step_limit"
                                                         : "continue_floating",
                           next);
        }
      }
      if (!next || visited + 1 == dimension_)
        break;
      indices_ = std::move(*next);
      path_visited_.insert(make_support(indices_));
    }

    const terminal_principal_trace terminal =
        verify_terminal_principal_pd(matrix);
    record_terminal_principal_factorization(visited_steps, terminal);
    last_walk_reached_kkt_ = walk_reached_kkt_;
    commit_walk_closures();
    return true;
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
        COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("pair_upward", 2);
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
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
        COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("high_float_reject",
                                            indices_.size());
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
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
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
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
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
      ++combined_ray_sweep_count_;
#endif
      COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("combined_ray", ray + 1);
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
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
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

#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
    if (captured_lower_ != nullptr) {
      *captured_lower_ = lower;
      *captured_upper_ = upper;
      return true;
    }
#endif
    if (defer_seed_certificate_) {
      assert(!deferred_seed_certificate_);
      deferred_seed_certificate_.emplace(buffered_seed_certificate{
          lower, upper, seed_certificate_type::dickinson, "dickinson",
          upper_size - lower_size, upper_size});
      return true;
    }
    supports_->add_interval(lower, upper);
    if (diagnostics_.active())
      diagnostics_.certificate(upper_size - lower_size, upper_size);
    record_interval_certificate(lower, upper);
    COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("dickinson", indices_.size());
    return true;
  }

  bool add_curvature_exclusion() {
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
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
    if (defer_seed_certificate_) {
      assert(!deferred_seed_certificate_);
      deferred_seed_certificate_.emplace(buffered_seed_certificate{
          lower, ceiling, seed_certificate_type::upward, "support_curvature",
          product_.size() - indices_.size(), product_.size()});
      return;
    }
    supports_->add_interval(lower, ceiling);
    if (diagnostics_.active())
      diagnostics_.certificate(product_.size() - indices_.size(),
                               product_.size());
    record_closure_certificate("support_curvature", "upward",
                               certificate_frontier_, indices_, false);
    COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("support_upward", indices_.size());
  }

  void add_downward_closure(std::string_view kind) {
    support floor(product_.size());
    support upper(product_.size());
    for (const size_t index : indices_)
      upper.set(index);
    if (defer_seed_certificate_) {
      assert(!deferred_seed_certificate_);
      deferred_seed_certificate_.emplace(buffered_seed_certificate{
          floor, upper, seed_certificate_type::downward, kind, 0, 0});
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
      ++downward_count_;
#endif
      return;
    }
    supports_->add_interval(floor, upper);
    diagnostics_.certificate();
    record_closure_certificate(kind, "downward", certificate_frontier_,
                               indices_, certificate_frontier_ == "high");
    COPOSIT_IMPROVED_NBC_B9_DIAGNOSTICS("downward", indices_.size());
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
    ++downward_count_;
#endif
  }

  bool finish(bool result) {
    diagnostics_.finish();
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
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

#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
  void publish_test_counters() const noexcept {
    last_optimized_certificate_count = optimized_certificate_count_;
    last_combined_ray_sweep_count = combined_ray_sweep_count_;
    last_combined_ray_improvement_count = combined_ray_improvement_count_;
    last_pair_curvature_exclusion_count = pair_curvature_exclusion_count_;
    last_support_curvature_exclusion_count = support_curvature_exclusion_count_;
    last_downward_count = downward_count_;
    last_high_float_rejection_count = high_float_rejection_count_;
  }
#endif

  size_t dimension_;
  fraction_free_ldlt_factorization factorization_;
  floating_positive_semidefinite_filter floating_filter_;
  double_matrix floating_matrix_;
  double_matrix floating_reduced_;
  std::vector<double> floating_rhs_;
  std::vector<double> floating_solution_;
  std::vector<double> floating_products_;
  std::vector<int> floating_pivots_;
  double floating_payoff_ = 0.0;
  matrix_integer principal_;
  matrix_integer solution_;
  matrix_integer walk_reduced_;
  matrix_integer walk_rhs_;
  matrix_integer walk_original_rhs_;
  matrix_integer walk_solution_;
  matrix_integer walk_products_;
  matrix_integer walk_basis_;
  integer walk_denominator_;
  integer walk_payoff_;
  std::vector<integer> walk_direction_;
  matrix_integer directions_;
  matrix_integer direction_products_;
  matrix_integer combined_directions_;
  matrix_integer combined_products_;
  std::vector<integer> product_;
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
  std::set<support> path_visited_;
  std::set<std::string> kkt_signatures_;
  std::vector<buffered_walk_closure> buffered_walk_closures_;
  walk_choice_trace walk_choice_trace_;
  std::vector<size_t> active_walk_seed_;
  std::optional<buffered_seed_certificate> deferred_seed_certificate_;
  std::mt19937_64 random_;
  size_t walk_count_ = 0;
  size_t active_walk_id_ = 0;
  size_t supports_since_walk_ = 0;
  size_t walk_gap_ = 1;
  bool next_walk_from_low_ = true;
  bool initial_low_walk_done_ = false;
  bool initial_high_walk_done_ = false;
  bool walk_reached_kkt_ = false;
  bool last_walk_reached_kkt_ = false;
  bool defer_seed_certificate_ = false;
#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
  size_t optimized_certificate_count_ = 0;
  size_t combined_ray_sweep_count_ = 0;
  size_t combined_ray_improvement_count_ = 0;
  size_t pair_curvature_exclusion_count_ = 0;
  size_t support_curvature_exclusion_count_ = 0;
  size_t downward_count_ = 0;
  size_t high_float_rejection_count_ = 0;
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

#ifdef COPOSIT_IMPROVED_NBC_B9_TESTING
bool improved_nbc_b9_prefers_negative_singular_orientation_for_testing(
    size_t positive_products, size_t negative_products) noexcept {
  return negative_orientation_has_larger_upper(positive_products,
                                               negative_products);
}

size_t improved_nbc_b9_optimized_certificate_count_for_testing() noexcept {
  return last_optimized_certificate_count;
}

size_t improved_nbc_b9_combined_ray_sweep_count_for_testing() noexcept {
  return last_combined_ray_sweep_count;
}

size_t improved_nbc_b9_combined_ray_improvement_count_for_testing() noexcept {
  return last_combined_ray_improvement_count;
}

size_t improved_nbc_b9_pair_curvature_exclusion_count_for_testing() noexcept {
  return last_pair_curvature_exclusion_count;
}

size_t
improved_nbc_b9_support_curvature_exclusion_count_for_testing() noexcept {
  return last_support_curvature_exclusion_count;
}

size_t improved_nbc_b9_pair_upward_count_for_testing() noexcept {
  return last_pair_curvature_exclusion_count;
}

size_t improved_nbc_b9_support_upward_count_for_testing() noexcept {
  return last_support_curvature_exclusion_count;
}

size_t improved_nbc_b9_downward_count_for_testing() noexcept {
  return last_downward_count;
}

size_t improved_nbc_b9_high_float_rejection_count_for_testing() noexcept {
  return last_high_float_rejection_count;
}

bool improved_nbc_b9_floating_psd_candidate_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  floating_positive_semidefinite_filter filter(matrix.rows());
  filter.prepare(matrix);
  return filter.looks_positive_semidefinite(indices);
}

bool improved_nbc_b9_reduced_hessian_is_positive_definite_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .reduced_hessian_is_positive_definite_for_testing(matrix, indices);
}

size_t improved_nbc_b9_no_hiding_actions_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .no_hiding_actions_for_testing(matrix, indices);
}

bool improved_nbc_b9_kkt_walk_for_testing(const matrix_integer &matrix,
                                          const std::vector<size_t> &seed) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .kkt_walk_for_testing(matrix, seed);
}

size_t improved_nbc_b9_shortlist_limit_for_testing(size_t matrix_dimension,
                                                   size_t support_dimension) {
  return ray_shortlist_limit(matrix_dimension, support_dimension);
}

size_t improved_nbc_b9_next_walk_gap_for_testing(size_t dimension,
                                                 size_t current_gap,
                                                 bool reached_kkt) noexcept {
  return next_walk_gap(dimension, current_gap, reached_kkt);
}

bool improved_nbc_b9_prefers_ray_candidate_for_testing(
    size_t candidate_upper, size_t candidate_width, size_t candidate_gains,
    size_t candidate_losses, size_t current_upper, size_t current_width,
    size_t current_gains, size_t current_losses) {
  return better_ray_candidate(
      {candidate_width, candidate_upper}, candidate_gains, candidate_losses,
      true, {current_width, current_upper}, current_gains, current_losses);
}

bool improved_nbc_b9_check_support_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(),
                           copositivity_mode::strictly_copositive)
      .check_support_for_testing(matrix, indices);
}

bool improved_nbc_b9_certificate_for_testing(const matrix_integer &matrix,
                                             const std::vector<size_t> &indices,
                                             support &lower, support &upper) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .optimize_support_for_testing(matrix, indices, lower, upper);
}

size_t improved_nbc_b9_fixed_support_upper_size_for_testing() noexcept {
  return last_fixed_support_upper_size;
}

bool count_uncovered_support(void *opaque, const std::vector<size_t> &) {
  ++*static_cast<size_t *>(opaque);
  return true;
}

size_t improved_nbc_b9_uncovered_count(
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

size_t improved_nbc_b9_pair_exclusion_uncovered_count(size_t dimension,
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

size_t improved_nbc_b9_uncovered_count(
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

size_t improved_nbc_b9_interval_clause_size(size_t dimension,
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
