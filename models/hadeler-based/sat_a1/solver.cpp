#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cadical.hpp>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
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
  size_t coordinate = 0;
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

#ifdef COPOSIT_SAT_A1_TESTING
size_t last_optimized_certificate_count = 0;
size_t last_combined_ray_sweep_count = 0;
size_t last_combined_ray_improvement_count = 0;
size_t last_fixed_support_upper_size = 0;
size_t last_nondominated_upper_count = 0;
#endif

class timeout_terminator final : public CaDiCaL::Terminator {
public:
  bool terminate() override { return timeout_pending(); }
};

class interval_sat {
public:
  explicit interval_sat(const support_context& context) : context_(context)
        , dimension_(context.dimension()) {
    if (dimension_ > static_cast<size_t>(std::numeric_limits<int>::max() - 1))
      throw std::overflow_error(
          "SAT variable count exceeds CaDiCaL's integer literal range");
    next_variable_ = static_cast<int>(dimension_) + 1;

    if (!solver_.configure("sat"))
      throw std::runtime_error(
          "CaDiCaL lacks its satisfiable-instance configuration");
    if (!solver_.set("ilb", 2))
      throw std::runtime_error("CaDiCaL lacks incremental lazy backtracking");
    solver_.connect_terminator(&terminator_);

    std::vector<int> wires;
    size_t padded_dimension = 1;
    while (padded_dimension < dimension_) {
      if (padded_dimension >
          static_cast<size_t>(std::numeric_limits<int>::max()) / 2)
        throw std::overflow_error("SAT cardinality network is too large");
      padded_dimension *= 2;
    }
    wires.reserve(padded_dimension);
    for (size_t index = 0; index < dimension_; ++index)
      wires.push_back(variable(index));

    if (padded_dimension != dimension_) {
      const int constant_false = new_variable();
      add_clause({-constant_false});
      wires.resize(padded_dimension, constant_false);
    }

    bitonic_sort(wires, 0, wires.size(), true);
    cardinality_outputs_.assign(wires.begin(), wires.begin() + dimension_);
  }

  void start_cardinality(size_t cardinality) noexcept {
    cardinality_ = cardinality;
  }

  bool take_first(std::vector<size_t> &indices) {
    assert(cardinality_ >= 1 && cardinality_ <= dimension_);
    solver_.assume(cardinality_outputs_[cardinality_ - 1]);
    if (cardinality_ < dimension_)
      solver_.assume(-cardinality_outputs_[cardinality_]);

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
    assert(indices.size() == cardinality_);
    return true;
  }

  void add_interval(const support &lower, const support &upper) {
    size_t upper_size = 0;
#ifdef COPOSIT_SAT_A1_TESTING
    last_interval_clause_size_ = 0;
#endif
    for (size_t index = 0; index < dimension_; ++index) {
      const bool in_upper = context_.contains(upper, index);
      if (in_upper)
        ++upper_size;
      if (context_.contains(lower, index)) {
        solver_.add(-variable(index));
#ifdef COPOSIT_SAT_A1_TESTING
        ++last_interval_clause_size_;
#endif
      } else if (!in_upper) {
        solver_.add(variable(index));
#ifdef COPOSIT_SAT_A1_TESTING
        ++last_interval_clause_size_;
#endif
      }
    }
    if (upper_size < dimension_) {
      solver_.add(cardinality_outputs_[upper_size]);
#ifdef COPOSIT_SAT_A1_TESTING
      ++last_interval_clause_size_;
#endif
    }
    solver_.add(0);
    ++interval_count_;
  }

  size_t interval_count() const noexcept { return interval_count_; }
#ifdef COPOSIT_SAT_A1_TESTING
  size_t last_interval_clause_size() const noexcept {
    return last_interval_clause_size_;
  }
#endif

private:
  int variable(size_t index) const noexcept {
    return static_cast<int>(index) + 1;
  }

  int new_variable() {
    if (next_variable_ == std::numeric_limits<int>::max())
      throw std::overflow_error(
          "SAT cardinality network exceeds CaDiCaL's integer literal range");
    if ((next_variable_ & 4095) == 0)
      timeout_checkpoint();
    return next_variable_++;
  }

  void add_clause(std::initializer_list<int> literals) {
    for (const int literal : literals)
      solver_.add(literal);
    solver_.add(0);
  }

  std::pair<int, int> comparator(int first, int second) {
    const int high = new_variable();
    const int low = new_variable();
    add_clause({-first, high});
    add_clause({-second, high});
    add_clause({first, second, -high});
    add_clause({first, -low});
    add_clause({second, -low});
    add_clause({-first, -second, low});
    return {high, low};
  }

  void compare_exchange(std::vector<int> &wires, size_t first, size_t second,
                        bool descending) {
    const auto [high, low] = comparator(wires[first], wires[second]);
    wires[first] = descending ? high : low;
    wires[second] = descending ? low : high;
  }

  void bitonic_merge(std::vector<int> &wires, size_t first, size_t count,
                     bool descending) {
    if (count < 2)
      return;
    const size_t half = count / 2;
    for (size_t index = first; index < first + half; ++index)
      compare_exchange(wires, index, index + half, descending);
    bitonic_merge(wires, first, half, descending);
    bitonic_merge(wires, first + half, half, descending);
  }

  void bitonic_sort(std::vector<int> &wires, size_t first, size_t count,
                    bool descending) {
    if (count < 2)
      return;
    const size_t half = count / 2;
    bitonic_sort(wires, first, half, !descending);
    bitonic_sort(wires, first + half, half, descending);
    bitonic_merge(wires, first, count, descending);
  }

  const support_context& context_;


  size_t dimension_;
  int next_variable_ = 1;
  size_t cardinality_ = 0;
  size_t interval_count_ = 0;
#ifdef COPOSIT_SAT_A1_TESTING
  size_t last_interval_clause_size_ = 0;
#endif
  timeout_terminator terminator_;
  CaDiCaL::Solver solver_;
  std::vector<int> cardinality_outputs_;
};

struct coverage_score {
  size_t width = 0;
  size_t upper_size = 0;
};

struct upper_endpoint {
  support upper;
  size_t upper_size = 0;
};

size_t endpoint_budget(size_t dimension) {
  const size_t additional = static_cast<size_t>(
      std::ceil(std::sqrt(static_cast<long double>(dimension)) / 2.0L));
  return std::min<size_t>(8, std::max<size_t>(2, 1 + additional));
}

struct intersection_term {
  support upper;
  bool subtract = false;
};

void add_selected_upper(support_context &context, std::vector<intersection_term> &terms,
                        const support &upper) {
  const size_t old_size = terms.size();
  terms.reserve(old_size * 2);
  for (size_t index = 0; index < old_size; ++index) {
    support intersection = context.clone(terms[index].upper);
    context.intersect(intersection, upper);
    terms.push_back({std::move(intersection), !terms[index].subtract});
  }
}

integer marginal_coverage(support_context &context, const support &candidate, size_t anchor_size,
                          const std::vector<intersection_term> &terms) {
  integer result;
  integer contribution;
  support intersection = context.make();
  for (size_t index = 0; index < terms.size(); ++index) {
    if ((index & 255U) == 0)
      timeout_checkpoint();
    context.copy(intersection, candidate);
    context.intersect(intersection, terms[index].upper);
    const size_t intersection_size = context.count(intersection);
    assert(intersection_size >= anchor_size);
    contribution.set_one();
    fmpz_mul_2exp(contribution.native_handle(), contribution.native_handle(),
                  intersection_size - anchor_size);
    if (terms[index].subtract)
      result -= contribution;
    else
      result += contribution;
  }
  context.release(std::move(intersection));
  return result;
}

bool retain_maximal_upper(support_context &context, std::vector<upper_endpoint> &endpoints,
                          const support &candidate, size_t candidate_size) {
  for (size_t index = 0; index < endpoints.size(); ++index) {
    if ((index & 255U) == 0)
      timeout_checkpoint();
    if (candidate_size <= endpoints[index].upper_size &&
        context.is_subset_of(candidate, endpoints[index].upper))
      return false;
  }

  size_t write = 0;
  for (size_t read = 0; read < endpoints.size(); ++read) {
    if ((read & 255U) == 0)
      timeout_checkpoint();
    if (endpoints[read].upper_size <= candidate_size &&
        context.is_subset_of(endpoints[read].upper, candidate)) {
      context.release(std::move(endpoints[read].upper));
      continue;
    }
    if (write != read)
      endpoints[write] = std::move(endpoints[read]);
    ++write;
  }
  endpoints.erase(endpoints.begin() + static_cast<std::ptrdiff_t>(write),
                  endpoints.end());
  endpoints.push_back({context.clone(candidate), candidate_size});
  return true;
}

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
      : support_context_(dimension)
        , factorization_(dimension), product_(dimension), sweep_upper_(support_context_.make()),
        shortlist_limit_(ray_shortlist_limit(dimension, dimension)),
        mode_(mode), diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
    ray_shortlist_.reserve(shortlist_limit_);
    shortlist_uppers_.reserve(shortlist_limit_);
    upper_endpoints_.reserve(shortlist_limit_);
    for (size_t index = 0; index < shortlist_limit_; ++index)
      shortlist_uppers_.push_back(support_context_.make());
  }

  dickinson_checker(size_t dimension,
                    copositivity_classification &classification)
      : support_context_(dimension)
        , factorization_(dimension), product_(dimension), sweep_upper_(support_context_.make()),
        shortlist_limit_(ray_shortlist_limit(dimension, dimension)),
        mode_(copositivity_mode::copositive), classification_(&classification),
        diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
    ray_shortlist_.reserve(shortlist_limit_);
    shortlist_uppers_.reserve(shortlist_limit_);
    upper_endpoints_.reserve(shortlist_limit_);
    for (size_t index = 0; index < shortlist_limit_; ++index)
      shortlist_uppers_.push_back(support_context_.make());
  }

  bool check(const matrix_integer &matrix) {
#ifdef COPOSIT_SAT_A1_TESTING
    optimized_certificate_count_ = 0;
    combined_ray_sweep_count_ = 0;
    combined_ray_improvement_count_ = 0;
#endif
    supports_.emplace(support_context_);
    for (size_t subset_dimension = 1; subset_dimension <= matrix.rows();
         ++subset_dimension) {
      diagnostics_.stage(subset_dimension);
      supports_->start_cardinality(subset_dimension);
      while (supports_->take_first(indices_)) {
        timeout_checkpoint();
        diagnostics_.visit_support();
        diagnostics_.secondary();
        COPOSIT_SAT_A1_DIAGNOSTICS("process", subset_dimension);
        if (!process_subset(matrix)) {
          diagnostics_.finish();
#ifdef COPOSIT_SAT_A1_TESTING
          publish_test_counters();
#endif
          return false;
        }
      }
    }

    diagnostics_.finish();
#ifdef COPOSIT_SAT_A1_TESTING
    publish_test_counters();
#endif
    return true;
  }

#ifdef COPOSIT_SAT_A1_TESTING
  bool check_support_for_testing(const matrix_integer &matrix,
                                 const std::vector<size_t> &indices) {
    support lower = support_context_.make();
    support upper = support_context_.make();
    const bool result =
        optimize_support_for_testing(matrix, indices, lower, upper);
    last_fixed_support_upper_size = 0;
    for (size_t index = 0; index < matrix.rows(); ++index)
      last_fixed_support_upper_size += support_context_.contains(upper, index);
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

  size_t
  interval_count_for_support_for_testing(const matrix_integer &matrix,
                                         const std::vector<size_t> &indices) {
    optimized_certificate_count_ = 0;
    combined_ray_sweep_count_ = 0;
    combined_ray_improvement_count_ = 0;
    indices_ = indices;
    supports_.emplace(support_context_);
    if (!process_subset(matrix))
      return 0;
    publish_test_counters();
    return supports_->interval_count();
  }
#endif

private:
  bool process_subset(const matrix_integer &matrix) {
    for (upper_endpoint &endpoint : upper_endpoints_) support_context_.release(std::move(endpoint.upper));
    upper_endpoints_.clear();
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

    calculate_nonsingular_product(matrix, solution_, 0, denominator, product_);
    current_score_ = score(solution_, 0, product_);
    retain_current_upper();
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

  void retain_current_upper() {
    support_context_.clear(sweep_upper_);
    size_t upper_size = 0;
    for (size_t row = 0; row < product_.size(); ++row) {
      if (product_[row].sign() < 0)
        continue;
      support_context_.set(sweep_upper_, row);
      ++upper_size;
    }
    retain_maximal_upper(support_context_, upper_endpoints_, sweep_upper_, upper_size);
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
#ifdef COPOSIT_SAT_A1_TESTING
    ++optimized_certificate_count_;
#endif
    return true;
  }

  void find_breakpoints(size_t direction) {
    breakpoint_events_.clear();
    for (size_t row = 0; row < solution_.rows(); ++row)
      add_positive_breakpoint(solution_(row, 0), directions_(row, direction),
                              true, row);
    for (size_t row = 0; row < product_.size(); ++row)
      add_positive_breakpoint(product_[row],
                              direction_products_(row, direction), false, row);

    std::sort(breakpoint_events_.begin(), breakpoint_events_.end(),
              [](const auto &left, const auto &right) {
                return ratio_less(left.root, right.root);
              });
  }

  void add_positive_breakpoint(integer::const_reference base,
                               integer::const_reference direction,
                               bool solution_entry, size_t coordinate) {
    if (base.is_zero() || direction.is_zero() ||
        base.sign() == direction.sign())
      return;
    breakpoint_event event;
    event.root.numerator.set_abs(base);
    event.root.denominator.set_abs(direction);
    event.solution_entry = solution_entry;
    event.direction_sign = direction.sign();
    event.coordinate = coordinate;
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
    support_context_.clear(sweep_upper_);
    for (size_t row = 0; row < product_.size(); ++row) {
      const int base_sign = product_[row].sign();
      const int direction_sign = direction_products_(row, direction).sign();
      const bool in_upper =
          base_sign > 0 || (base_sign == 0 && direction_sign >= 0);
      interval_upper_size += in_upper;
      if (in_upper)
        support_context_.set(sweep_upper_, row);
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
          support_context_.set(sweep_upper_, event.coordinate);
        } else {
          ++negative_product_event_count;
        }
      }

      const size_t root_gain_size =
          interval_gain_size + positive_product_event_count;
      const size_t root_loss_size = interval_loss_size;
      if (positive_product_event_count > 0)
        retain_maximal_upper(support_context_, upper_endpoints_, sweep_upper_, root_upper_size);
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

      for (size_t index = group_begin; index < group_end; ++index) {
        const breakpoint_event &event = breakpoint_events_[index];
        if (!event.solution_entry && event.direction_sign < 0)
          support_context_.reset(sweep_upper_, event.coordinate);
      }

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
      const bool first_upper = support_context_.contains(shortlist_uppers_[first], row);
      const bool second_upper = support_context_.contains(shortlist_uppers_[second], row);
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
#ifdef COPOSIT_SAT_A1_TESTING
      ++combined_ray_sweep_count_;
#endif
      COPOSIT_SAT_A1_DIAGNOSTICS("combined_ray", ray + 1);
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
#ifdef COPOSIT_SAT_A1_TESTING
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
      else if (mode_ == copositivity_mode::strictly_copositive)
        return false;
    }

    retain_maximal_upper(support_context_, upper_endpoints_, upper, upper_size);

#ifdef COPOSIT_SAT_A1_TESTING
    if (captured_lower_ != nullptr) {
      support_context_.copy(*captured_lower_, lower);
      support_context_.copy(*captured_upper_, upper);
      support_context_.release(std::move(lower));
      support_context_.release(std::move(upper));
      return true;
    }
#endif
    support anchor = support_context_.make();
    for (const size_t index : indices_)
      support_context_.set(anchor, index);

    supports_->add_interval(lower, upper);
    if (diagnostics_.active())
      diagnostics_.certificate(upper_size - lower_size, upper_size);

    std::vector<intersection_term> intersections;
    intersections.push_back({support_context_.make(), false});
    support_context_.set_all(intersections.front().upper);
    add_selected_upper(support_context_, intersections, upper);

    std::vector<bool> selected(upper_endpoints_.size(), false);
    const size_t budget = endpoint_budget(product_.size());
    for (size_t retained = 1; retained < budget; ++retained) {
      size_t best_index = upper_endpoints_.size();
      integer best_marginal;
      for (size_t index = 0; index < upper_endpoints_.size(); ++index) {
        if (selected[index] || support_context_.equal(upper_endpoints_[index].upper, upper))
          continue;
        const integer marginal = marginal_coverage(
            support_context_, upper_endpoints_[index].upper, indices_.size(), intersections);
        if (marginal.sign() <= 0)
          continue;
        if (best_index == upper_endpoints_.size() ||
            marginal.compare(best_marginal) > 0 ||
            (marginal.compare(best_marginal) == 0 &&
             (upper_endpoints_[index].upper_size >
                  upper_endpoints_[best_index].upper_size ||
              (upper_endpoints_[index].upper_size ==
                   upper_endpoints_[best_index].upper_size &&
               support_context_.less(upper_endpoints_[index].upper,
                                     upper_endpoints_[best_index].upper))))) {
          best_index = index;
          best_marginal = marginal;
        }
      }
      if (best_index == upper_endpoints_.size())
        break;

      selected[best_index] = true;
      const upper_endpoint &endpoint = upper_endpoints_[best_index];
      assert(endpoint.upper_size >= indices_.size());
      supports_->add_interval(anchor, endpoint.upper);
      if (diagnostics_.active())
        diagnostics_.certificate(endpoint.upper_size - indices_.size(),
                                 endpoint.upper_size);
      if (retained + 1 < budget)
        add_selected_upper(support_context_, intersections, endpoint.upper);
    }
    COPOSIT_SAT_A1_DIAGNOSTICS("upper_antichain", upper_endpoints_.size());
    for (intersection_term &term : intersections) support_context_.release(std::move(term.upper));
    support_context_.release(std::move(anchor));
    support_context_.release(std::move(lower));
    support_context_.release(std::move(upper));
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

#ifdef COPOSIT_SAT_A1_TESTING
  void publish_test_counters() const noexcept {
    last_optimized_certificate_count = optimized_certificate_count_;
    last_combined_ray_sweep_count = combined_ray_sweep_count_;
    last_combined_ray_improvement_count = combined_ray_improvement_count_;
    last_nondominated_upper_count = upper_endpoints_.size();
  }
#endif

  support_context support_context_;

  fraction_free_ldlt_factorization factorization_;
  matrix_integer principal_;
  matrix_integer solution_;
  matrix_integer directions_;
  matrix_integer direction_products_;
  matrix_integer combined_directions_;
  matrix_integer combined_products_;
  std::vector<integer> product_;
  support sweep_upper_;
  size_t shortlist_limit_;
  std::vector<size_t> indices_;
  std::vector<breakpoint_event> breakpoint_events_;
  std::vector<ray_candidate> ray_shortlist_;
  std::vector<support> shortlist_uppers_;
  std::vector<upper_endpoint> upper_endpoints_;
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
  std::optional<interval_sat> supports_;
#ifdef COPOSIT_SAT_A1_TESTING
  size_t optimized_certificate_count_ = 0;
  size_t combined_ray_sweep_count_ = 0;
  size_t combined_ray_improvement_count_ = 0;
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

#ifdef COPOSIT_SAT_A1_TESTING
bool sat_a1_prefers_negative_singular_orientation_for_testing(
    size_t positive_products, size_t negative_products) noexcept {
  return negative_orientation_has_larger_upper(positive_products,
                                               negative_products);
}

size_t sat_a1_optimized_certificate_count_for_testing() noexcept {
  return last_optimized_certificate_count;
}

size_t sat_a1_combined_ray_sweep_count_for_testing() noexcept {
  return last_combined_ray_sweep_count;
}

size_t sat_a1_combined_ray_improvement_count_for_testing() noexcept {
  return last_combined_ray_improvement_count;
}

size_t sat_a1_shortlist_limit_for_testing(size_t matrix_dimension,
                                          size_t support_dimension) {
  return ray_shortlist_limit(matrix_dimension, support_dimension);
}

size_t sat_a1_endpoint_budget_for_testing(size_t matrix_dimension) {
  return endpoint_budget(matrix_dimension);
}

bool sat_a1_prefers_ray_candidate_for_testing(
    size_t candidate_upper, size_t candidate_width, size_t candidate_gains,
    size_t candidate_losses, size_t current_upper, size_t current_width,
    size_t current_gains, size_t current_losses) {
  return better_ray_candidate(
      {candidate_width, candidate_upper}, candidate_gains, candidate_losses,
      true, {current_width, current_upper}, current_gains, current_losses);
}

bool sat_a1_check_support_for_testing(const matrix_integer &matrix,
                                      const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(),
                           copositivity_mode::strictly_copositive)
      .check_support_for_testing(matrix, indices);
}

bool sat_a1_certificate_for_testing(const matrix_integer &matrix,
                                    const std::vector<size_t> &indices,
                                    support &lower, support &upper) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .optimize_support_for_testing(matrix, indices, lower, upper);
}

size_t sat_a1_fixed_support_upper_size_for_testing() noexcept {
  return last_fixed_support_upper_size;
}

size_t sat_a1_nondominated_upper_count_for_testing() noexcept {
  return last_nondominated_upper_count;
}

size_t sat_a1_interval_count_for_support_for_testing(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .interval_count_for_support_for_testing(matrix, indices);
}

size_t sat_a1_maximal_upper_count_for_testing(
    size_t dimension, const std::vector<uint64_t> &candidates) {
  assert(dimension <= 64);
  support_context context(dimension);
  std::vector<upper_endpoint> endpoints;
  for (const uint64_t mask : candidates) {
    support candidate = context.make();
    size_t candidate_size = 0;
    for (size_t bit = 0; bit < dimension; ++bit) {
      if ((mask & (uint64_t{1} << bit)) == 0)
        continue;
      context.set(candidate, bit);
      ++candidate_size;
    }
    retain_maximal_upper(context, endpoints, candidate, candidate_size);
    context.release(std::move(candidate));
  }
  return endpoints.size();
}

size_t sat_a1_uncovered_count(
    size_t dimension, size_t cardinality,
    const std::vector<std::pair<uint64_t, uint64_t>> &intervals) {
  support_context support_context_(dimension);
    interval_sat diagram(support_context_);
  for (const auto &[lower_mask, upper_mask] : intervals) {
    support lower = support_context_.make();
    support upper = support_context_.make();
    for (size_t bit = 0; bit < dimension; ++bit) {
      if ((lower_mask & (uint64_t{1} << bit)) != 0)
        support_context_.set(lower, bit);
      if ((upper_mask & (uint64_t{1} << bit)) != 0)
        support_context_.set(upper, bit);
    }
    diagram.add_interval(lower, upper);
  }

  diagram.start_cardinality(cardinality);
  std::vector<size_t> indices;
  size_t count = 0;
  while (diagram.take_first(indices)) {
    support exact = support_context_.make();
    for (const size_t index : indices)
      support_context_.set(exact, index);
    diagram.add_interval(exact, exact);
        support_context_.release(std::move(exact));
    ++count;
  }
  return count;
}

size_t sat_a1_interval_clause_size(size_t dimension, uint64_t lower_mask,
                                   uint64_t upper_mask) {
  support_context support_context_(dimension);
    interval_sat diagram(support_context_);
  support lower = support_context_.make();
  support upper = support_context_.make();
  for (size_t bit = 0; bit < dimension; ++bit) {
    if ((lower_mask & (uint64_t{1} << bit)) != 0)
      support_context_.set(lower, bit);
    if ((upper_mask & (uint64_t{1} << bit)) != 0)
      support_context_.set(upper, bit);
  }
  diagram.add_interval(lower, upper);
  return diagram.last_interval_clause_size();
}
#endif

} // namespace coposit::model
