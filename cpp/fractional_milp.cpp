#include <coposit/fractional_milp.hpp>

#include <coposit/timeout.hpp>

#include <flint/fmpq.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace coposit::fractional_milp {

fraction::fraction() = default;

fraction::fraction(slong value) : numerator_(value) {}

fraction::fraction(integer numerator, integer denominator)
    : numerator_(std::move(numerator)), denominator_(std::move(denominator)) {
  if (denominator_.is_zero())
    throw std::invalid_argument("fraction denominator must be nonzero");
  fmpq_t value;
  fmpq_init(value);
  fmpq_set_fmpz_frac(value, numerator_.native_handle(),
                     denominator_.native_handle());
  numerator_ = integer(integer::const_reference(fmpq_numref(value)));
  denominator_ = integer(integer::const_reference(fmpq_denref(value)));
  fmpq_clear(value);
}

double fraction::to_double() const noexcept {
  fmpq_t value;
  fmpq_init(value);
  fmpq_set_fmpz_frac(value, numerator_.native_handle(),
                     denominator_.native_handle());
  const double result = fmpq_get_d(value);
  fmpq_clear(value);
  return result;
}

std::string fraction::to_string() const {
  if (denominator_.is_one())
    return numerator_.to_string();
  return numerator_.to_string() + '/' + denominator_.to_string();
}

problem::problem(size_t variable_count)
    : variable_types_(variable_count, variable_type::continuous),
      upper_bounds_(variable_count), has_upper_bound_(variable_count, false),
      objective_(variable_count) {
  if (variable_count == 0)
    throw std::invalid_argument("fractional MILP needs at least one variable");
}

void problem::set_variable_type(size_t variable, variable_type type) {
  if (variable >= variable_count())
    throw std::out_of_range("fractional MILP variable index");
  variable_types_[variable] = type;
}

void problem::set_upper_bound(size_t variable, integer upper_bound) {
  if (variable >= variable_count())
    throw std::out_of_range("fractional MILP variable index");
  if (upper_bound.sign() < 0)
    throw std::invalid_argument(
        "fractional MILP upper bound must be nonnegative");
  upper_bounds_[variable] = std::move(upper_bound);
  has_upper_bound_[variable] = true;
}

void problem::set_objective_coefficient(size_t variable, integer coefficient) {
  if (variable >= variable_count())
    throw std::out_of_range("fractional MILP variable index");
  objective_[variable] = std::move(coefficient);
}

void problem::add_constraint(std::vector<integer> coefficients,
                             constraint_sense sense, integer right_hand_side) {
  if (coefficients.size() != variable_count())
    throw std::invalid_argument(
        "fractional MILP constraint has wrong dimension");
  constraints_.push_back(
      {std::move(coefficients), sense, std::move(right_hand_side)});
}

namespace {

class exact_number {
public:
  exact_number() noexcept { fmpq_init(value_); }
  explicit exact_number(slong value) noexcept {
    fmpq_init(value_);
    fmpq_set_si(value_, value, 1);
  }
  exact_number(const exact_number &other) noexcept {
    fmpq_init(value_);
    fmpq_set(value_, other.value_);
  }
  exact_number(exact_number &&other) noexcept {
    fmpq_init(value_);
    fmpq_swap(value_, other.value_);
  }
  ~exact_number() noexcept { fmpq_clear(value_); }

  exact_number &operator=(const exact_number &other) noexcept {
    if (this != &other)
      fmpq_set(value_, other.value_);
    return *this;
  }

  exact_number &operator=(exact_number &&other) noexcept {
    if (this != &other)
      fmpq_swap(value_, other.value_);
    return *this;
  }

  void set(integer::const_reference value) noexcept {
    fmpq_set_fmpz(value_, value.native_handle());
  }
  void set(const exact_number &value) noexcept {
    fmpq_set(value_, value.value_);
  }
  void negate() noexcept { fmpq_neg(value_, value_); }
  void invert(const exact_number &value) noexcept {
    fmpq_inv(value_, value.value_);
  }
  void multiply(const exact_number &value) noexcept {
    fmpq_mul(value_, value_, value.value_);
  }
  void set_product(const exact_number &left,
                   const exact_number &right) noexcept {
    fmpq_mul(value_, left.value_, right.value_);
  }
  void submul(const exact_number &left, const exact_number &right) noexcept {
    fmpq_submul(value_, left.value_, right.value_);
  }
  void divide(const exact_number &numerator,
              const exact_number &denominator) noexcept {
    fmpq_div(value_, numerator.value_, denominator.value_);
  }

  int sign() const noexcept { return fmpq_sgn(value_); }
  bool is_zero() const noexcept { return fmpq_is_zero(value_); }
  int compare(const exact_number &other) const noexcept {
    return fmpq_cmp(value_, other.value_);
  }
  int compare_si(slong value) const noexcept {
    return fmpq_cmp_si(value_, value);
  }
  double to_double() const noexcept { return fmpq_get_d(value_); }
  const fmpz *numerator() const noexcept { return fmpq_numref(value_); }
  const fmpz *denominator() const noexcept { return fmpq_denref(value_); }

private:
  fmpq_t value_;
};

struct integer_row {
  std::vector<integer> coefficients;
  integer right_hand_side;
};

enum class lp_status { optimal, infeasible, unbounded, interrupted };

struct floating_lp_result {
  lp_status status = lp_status::interrupted;
  std::vector<double> point;
  double objective = 0.0;
};

struct exact_lp_result {
  lp_status status = lp_status::interrupted;
  std::vector<exact_number> point;
  exact_number objective;
};

bool stopped(std::chrono::steady_clock::time_point deadline) noexcept {
  return timeout_pending() || std::chrono::steady_clock::now() >= deadline;
}

size_t tableau_cells(size_t rows, size_t columns) {
  if (columns > static_cast<size_t>(std::numeric_limits<int>::max()) ||
      rows > static_cast<size_t>(std::numeric_limits<int>::max()) - columns ||
      columns > std::numeric_limits<size_t>::max() - 2 ||
      rows > std::numeric_limits<size_t>::max() - 2 ||
      rows + 2 > std::numeric_limits<size_t>::max() / (columns + 2))
    throw std::overflow_error("fractional MILP tableau is too large");
  return (rows + 2) * (columns + 2);
}

class floating_simplex {
public:
  floating_simplex(size_t rows, size_t columns,
                   std::chrono::steady_clock::time_point deadline)
      : rows_(rows), columns_(columns), stride_(columns + 2),
        tableau_(tableau_cells(rows, columns), 0.0), basic_(rows),
        nonbasic_(columns + 1), deadline_(deadline) {
    for (size_t row = 0; row < rows_; ++row) {
      basic_[row] = static_cast<int>(columns_ + row);
      at(row, columns_) = -1.0;
    }
    for (size_t column = 0; column < columns_; ++column)
      nonbasic_[column] = static_cast<int>(column);
    nonbasic_[columns_] = -1;
    at(rows_ + 1, columns_) = 1.0;
  }

  void set_coefficient(size_t row, size_t column, double value) {
    at(row, column) = value;
  }
  void set_rhs(size_t row, double value) { at(row, columns_ + 1) = value; }
  void set_objective(size_t column, double value) {
    at(rows_, column) = -value;
  }

  floating_lp_result solve() {
    floating_lp_result result;
    if (rows_ > 0) {
      size_t pivot_row = 0;
      for (size_t row = 1; row < rows_; ++row)
        if (at(row, columns_ + 1) < at(pivot_row, columns_ + 1))
          pivot_row = row;
      if (at(pivot_row, columns_ + 1) < -epsilon) {
        pivot(pivot_row, columns_);
        if (!run(true)) {
          result.status =
              interrupted_ ? lp_status::interrupted : lp_status::infeasible;
          return result;
        }
        if (std::abs(at(rows_ + 1, columns_ + 1)) > epsilon) {
          result.status = lp_status::infeasible;
          return result;
        }
        remove_artificial_from_basis();
      }
    }
    if (!run(false)) {
      result.status =
          interrupted_ ? lp_status::interrupted : lp_status::unbounded;
      return result;
    }
    result.status = lp_status::optimal;
    result.point.assign(columns_, 0.0);
    for (size_t row = 0; row < rows_; ++row)
      if (basic_[row] >= 0 && static_cast<size_t>(basic_[row]) < columns_)
        result.point[static_cast<size_t>(basic_[row])] = at(row, columns_ + 1);
    result.objective = at(rows_, columns_ + 1);
    return result;
  }

private:
  static constexpr double epsilon = 1e-9;

  double &at(size_t row, size_t column) {
    return tableau_[row * stride_ + column];
  }
  double at(size_t row, size_t column) const {
    return tableau_[row * stride_ + column];
  }

  void remove_artificial_from_basis() {
    for (size_t row = 0; row < rows_; ++row) {
      if (basic_[row] != -1)
        continue;
      size_t column = 0;
      for (size_t candidate = 1; candidate <= columns_; ++candidate)
        if (std::abs(at(row, candidate)) > std::abs(at(row, column)))
          column = candidate;
      if (std::abs(at(row, column)) > epsilon)
        pivot(row, column);
    }
  }

  void pivot(size_t pivot_row, size_t pivot_column) {
    const double inverse = 1.0 / at(pivot_row, pivot_column);
    const double *pivot_data = tableau_.data() + pivot_row * stride_;
    for (size_t row = 0; row < rows_ + 2; ++row) {
      if (row == pivot_row)
        continue;
      double *row_data = tableau_.data() + row * stride_;
      const double factor = row_data[pivot_column] * inverse;
      for (size_t column = 0; column < stride_; ++column)
        if (column != pivot_column)
          row_data[column] -= pivot_data[column] * factor;
    }
    double *mutable_pivot_data = tableau_.data() + pivot_row * stride_;
    for (size_t column = 0; column < stride_; ++column)
      if (column != pivot_column)
        mutable_pivot_data[column] *= inverse;
    for (size_t row = 0; row < rows_ + 2; ++row)
      if (row != pivot_row)
        tableau_[row * stride_ + pivot_column] *= -inverse;
    mutable_pivot_data[pivot_column] = inverse;
    std::swap(basic_[pivot_row], nonbasic_[pivot_column]);
  }

  bool run(bool phase_one) {
    const size_t objective_row = phase_one ? rows_ + 1 : rows_;
    while (true) {
      if (stopped(deadline_)) {
        interrupted_ = true;
        return false;
      }
      size_t pivot_column = columns_ + 1;
      for (size_t column = 0; column <= columns_; ++column) {
        if (!phase_one && nonbasic_[column] == -1)
          continue;
        if (pivot_column == columns_ + 1 ||
            at(objective_row, column) <
                at(objective_row, pivot_column) - epsilon ||
            (std::abs(at(objective_row, column) -
                      at(objective_row, pivot_column)) <= epsilon &&
             nonbasic_[column] < nonbasic_[pivot_column]))
          pivot_column = column;
      }
      if (pivot_column == columns_ + 1 ||
          at(objective_row, pivot_column) >= -epsilon)
        return true;
      size_t pivot_row = rows_;
      for (size_t row = 0; row < rows_; ++row) {
        if (at(row, pivot_column) <= epsilon)
          continue;
        if (pivot_row == rows_) {
          pivot_row = row;
          continue;
        }
        const double ratio = at(row, columns_ + 1) / at(row, pivot_column);
        const double current =
            at(pivot_row, columns_ + 1) / at(pivot_row, pivot_column);
        if (ratio < current - epsilon ||
            (std::abs(ratio - current) <= epsilon &&
             basic_[row] < basic_[pivot_row]))
          pivot_row = row;
      }
      if (pivot_row == rows_)
        return false;
      pivot(pivot_row, pivot_column);
    }
  }

  size_t rows_;
  size_t columns_;
  size_t stride_;
  std::vector<double> tableau_;
  std::vector<int> basic_;
  std::vector<int> nonbasic_;
  std::chrono::steady_clock::time_point deadline_;
  bool interrupted_ = false;
};

class exact_simplex {
public:
  exact_simplex(size_t rows, size_t columns,
                std::chrono::steady_clock::time_point deadline)
      : rows_(rows), columns_(columns), stride_(columns + 2),
        tableau_(tableau_cells(rows, columns)), basic_(rows),
        nonbasic_(columns + 1), deadline_(deadline) {
    for (size_t row = 0; row < rows_; ++row) {
      basic_[row] = static_cast<int>(columns_ + row);
      at(row, columns_) = exact_number(-1);
    }
    for (size_t column = 0; column < columns_; ++column)
      nonbasic_[column] = static_cast<int>(column);
    nonbasic_[columns_] = -1;
    at(rows_ + 1, columns_) = exact_number(1);
  }

  void set_coefficient(size_t row, size_t column,
                       integer::const_reference value) {
    at(row, column).set(value);
  }
  void set_rhs(size_t row, integer::const_reference value) {
    at(row, columns_ + 1).set(value);
  }
  void set_objective(size_t column, integer::const_reference value) {
    at(rows_, column).set(value);
    at(rows_, column).negate();
  }

  exact_lp_result solve() {
    exact_lp_result result;
    if (rows_ > 0) {
      size_t pivot_row = 0;
      for (size_t row = 1; row < rows_; ++row)
        if (at(row, columns_ + 1).compare(at(pivot_row, columns_ + 1)) < 0)
          pivot_row = row;
      if (at(pivot_row, columns_ + 1).sign() < 0) {
        pivot(pivot_row, columns_);
        if (!run(true)) {
          result.status =
              interrupted_ ? lp_status::interrupted : lp_status::infeasible;
          return result;
        }
        if (!at(rows_ + 1, columns_ + 1).is_zero()) {
          result.status = lp_status::infeasible;
          return result;
        }
        remove_artificial_from_basis();
      }
    }
    if (!run(false)) {
      result.status =
          interrupted_ ? lp_status::interrupted : lp_status::unbounded;
      return result;
    }
    result.status = lp_status::optimal;
    result.point.assign(columns_, exact_number{});
    for (size_t row = 0; row < rows_; ++row)
      if (basic_[row] >= 0 && static_cast<size_t>(basic_[row]) < columns_)
        result.point[static_cast<size_t>(basic_[row])].set(
            at(row, columns_ + 1));
    result.objective.set(at(rows_, columns_ + 1));
    return result;
  }

private:
  exact_number &at(size_t row, size_t column) {
    return tableau_[row * stride_ + column];
  }
  const exact_number &at(size_t row, size_t column) const {
    return tableau_[row * stride_ + column];
  }

  void remove_artificial_from_basis() {
    for (size_t row = 0; row < rows_; ++row) {
      if (basic_[row] != -1)
        continue;
      size_t column = columns_ + 1;
      for (size_t candidate = 0; candidate <= columns_; ++candidate) {
        if (at(row, candidate).is_zero())
          continue;
        if (column == columns_ + 1 || nonbasic_[candidate] < nonbasic_[column])
          column = candidate;
      }
      if (column != columns_ + 1)
        pivot(row, column);
    }
  }

  void pivot(size_t pivot_row, size_t pivot_column) {
    exact_number inverse;
    inverse.invert(at(pivot_row, pivot_column));
    exact_number factor;
    for (size_t row = 0; row < rows_ + 2; ++row) {
      if (row == pivot_row)
        continue;
      factor.set_product(at(row, pivot_column), inverse);
      for (size_t column = 0; column < stride_; ++column)
        if (column != pivot_column)
          at(row, column).submul(at(pivot_row, column), factor);
    }
    for (size_t column = 0; column < stride_; ++column)
      if (column != pivot_column)
        at(pivot_row, column).multiply(inverse);
    for (size_t row = 0; row < rows_ + 2; ++row) {
      if (row == pivot_row)
        continue;
      at(row, pivot_column).multiply(inverse);
      at(row, pivot_column).negate();
    }
    at(pivot_row, pivot_column).set(inverse);
    std::swap(basic_[pivot_row], nonbasic_[pivot_column]);
  }

  bool run(bool phase_one) {
    const size_t objective_row = phase_one ? rows_ + 1 : rows_;
    exact_number ratio;
    exact_number current;
    while (true) {
      if (stopped(deadline_)) {
        interrupted_ = true;
        return false;
      }
      size_t pivot_column = columns_ + 1;
      for (size_t column = 0; column <= columns_; ++column) {
        if (!phase_one && nonbasic_[column] == -1)
          continue;
        const int comparison =
            pivot_column == columns_ + 1
                ? -1
                : at(objective_row, column)
                      .compare(at(objective_row, pivot_column));
        if (pivot_column == columns_ + 1 || comparison < 0 ||
            (comparison == 0 && nonbasic_[column] < nonbasic_[pivot_column]))
          pivot_column = column;
      }
      if (pivot_column == columns_ + 1 ||
          at(objective_row, pivot_column).sign() >= 0)
        return true;
      size_t pivot_row = rows_;
      for (size_t row = 0; row < rows_; ++row) {
        if (at(row, pivot_column).sign() <= 0)
          continue;
        if (pivot_row == rows_) {
          pivot_row = row;
          continue;
        }
        ratio.divide(at(row, columns_ + 1), at(row, pivot_column));
        current.divide(at(pivot_row, columns_ + 1),
                       at(pivot_row, pivot_column));
        const int comparison = ratio.compare(current);
        if (comparison < 0 ||
            (comparison == 0 && basic_[row] < basic_[pivot_row]))
          pivot_row = row;
      }
      if (pivot_row == rows_)
        return false;
      pivot(pivot_row, pivot_column);
    }
  }

  size_t rows_;
  size_t columns_;
  size_t stride_;
  std::vector<exact_number> tableau_;
  std::vector<int> basic_;
  std::vector<int> nonbasic_;
  std::chrono::steady_clock::time_point deadline_;
  bool interrupted_ = false;
};

integer negated(integer::const_reference value) {
  integer result(value);
  result.negate();
  return result;
}

class branch_and_bound {
public:
  branch_and_bound(const problem &input, solve_options options)
      : input_(input), options_(options), fixed_(input.variable_count(), -1) {
    for (size_t variable = 0; variable < input_.variable_count(); ++variable)
      if (input_.type(variable) == variable_type::binary)
        binary_variables_.push_back(variable);
  }

  solve_result run() {
    const search_status status = search();
    result_.status =
        status == search_status::interrupted ? solve_status::interrupted
        : status == search_status::unbounded ? solve_status::unbounded
        : status == search_status::positive_objective_found
            ? solve_status::positive_objective_found
        : incumbent_point_.empty()           ? solve_status::infeasible
                                             : solve_status::optimal;
    if (result_.status == solve_status::optimal ||
        result_.status == solve_status::positive_objective_found) {
      result_.point.reserve(incumbent_point_.size());
      for (const exact_number &value : incumbent_point_)
        result_.point.emplace_back(
            integer(integer::const_reference(value.numerator())),
            integer(integer::const_reference(value.denominator())));
      result_.objective = fraction(
          integer(integer::const_reference(incumbent_objective_.numerator())),
          integer(
              integer::const_reference(incumbent_objective_.denominator())));
    }
    return std::move(result_);
  }

private:
  enum class search_status {
    complete,
    positive_objective_found,
    unbounded,
    interrupted
  };

  std::vector<integer_row> make_rows() const {
    std::vector<integer_row> rows;
    rows.reserve(input_.constraints().size() * 2 + input_.variable_count() +
                 binary_variables_.size());
    for (const auto &constraint : input_.constraints()) {
      if (constraint.sense != constraint_sense::greater_equal)
        rows.push_back({constraint.coefficients, constraint.right_hand_side});
      if (constraint.sense != constraint_sense::less_equal) {
        std::vector<integer> coefficients;
        coefficients.reserve(input_.variable_count());
        for (const integer &coefficient : constraint.coefficients)
          coefficients.push_back(negated(coefficient));
        rows.push_back(
            {std::move(coefficients), negated(constraint.right_hand_side)});
      }
    }

    for (size_t variable = 0; variable < input_.variable_count(); ++variable) {
      bool has_bound = input_.has_upper_bound(variable);
      integer bound = has_bound ? input_.upper_bound(variable) : integer{};
      if (input_.type(variable) == variable_type::binary &&
          (!has_bound || bound.compare(integer(1)) > 0)) {
        bound = integer(1);
        has_bound = true;
      }
      if (has_bound) {
        std::vector<integer> coefficients(input_.variable_count());
        coefficients[variable] = integer(1);
        rows.push_back({std::move(coefficients), std::move(bound)});
      }
      if (fixed_[variable] < 0)
        continue;
      std::vector<integer> coefficients(input_.variable_count());
      if (fixed_[variable] == 0) {
        coefficients[variable] = integer(1);
        rows.push_back({std::move(coefficients), integer(0)});
      } else {
        coefficients[variable] = integer(-1);
        rows.push_back({std::move(coefficients), integer(-1)});
      }
    }
    return rows;
  }

  static slong largest_exponent(const std::vector<integer> &values,
                                const integer *extra = nullptr) {
    slong largest = 0;
    bool found = false;
    const auto consider = [&](integer::const_reference value) {
      if (value.is_zero())
        return;
      slong exponent = 0;
      value.to_dbl_2exp(exponent);
      if (!found || exponent > largest)
        largest = exponent;
      found = true;
    };
    for (const integer &value : values)
      consider(value);
    if (extra != nullptr)
      consider(*extra);
    return found ? largest : 0;
  }

  static double scaled_double(integer::const_reference value,
                              slong scale_exponent) {
    if (value.is_zero())
      return 0.0;
    slong exponent = 0;
    const double mantissa = value.to_dbl_2exp(exponent);
    const slong difference = exponent - scale_exponent;
    if (difference < -1074)
      return std::copysign(0.0, mantissa);
    if (difference > 1023)
      return std::copysign(std::numeric_limits<double>::infinity(), mantissa);
    return std::ldexp(mantissa, static_cast<int>(difference));
  }

  floating_lp_result solve_floating(const std::vector<integer_row> &rows) {
    ++result_.floating_lp_solves;
    floating_simplex lp(rows.size(), input_.variable_count(),
                        options_.deadline);
    for (size_t row = 0; row < rows.size(); ++row) {
      const slong scale =
          largest_exponent(rows[row].coefficients, &rows[row].right_hand_side);
      for (size_t column = 0; column < input_.variable_count(); ++column)
        lp.set_coefficient(
            row, column, scaled_double(rows[row].coefficients[column], scale));
      lp.set_rhs(row, scaled_double(rows[row].right_hand_side, scale));
    }
    objective_scale_ = largest_exponent(input_.objective());
    for (size_t column = 0; column < input_.variable_count(); ++column)
      lp.set_objective(
          column, scaled_double(input_.objective()[column], objective_scale_));
    return lp.solve();
  }

  exact_lp_result solve_exact(const std::vector<integer_row> &rows) {
    ++result_.exact_lp_solves;
    exact_simplex lp(rows.size(), input_.variable_count(), options_.deadline);
    for (size_t row = 0; row < rows.size(); ++row) {
      for (size_t column = 0; column < input_.variable_count(); ++column)
        lp.set_coefficient(row, column, rows[row].coefficients[column]);
      lp.set_rhs(row, rows[row].right_hand_side);
    }
    for (size_t column = 0; column < input_.variable_count(); ++column)
      lp.set_objective(column, input_.objective()[column]);
    return lp.solve();
  }

  size_t floating_branch(const std::vector<double> &point) const {
    size_t best = input_.variable_count();
    double best_distance = 1.0;
    for (const size_t variable : binary_variables_) {
      if (fixed_[variable] >= 0 || !std::isfinite(point[variable]))
        continue;
      const double value = point[variable];
      if (value <= integrality_epsilon || value >= 1.0 - integrality_epsilon)
        continue;
      const double distance = std::abs(value - 0.5);
      if (distance < best_distance) {
        best = variable;
        best_distance = distance;
      }
    }
    return best;
  }

  size_t exact_branch(const std::vector<exact_number> &point) const {
    for (const size_t variable : binary_variables_) {
      if (fixed_[variable] >= 0)
        continue;
      if (point[variable].compare_si(0) != 0 &&
          point[variable].compare_si(1) != 0)
        return variable;
    }
    return input_.variable_count();
  }

  size_t first_unfixed_binary() const {
    for (const size_t variable : binary_variables_)
      if (fixed_[variable] < 0)
        return variable;
    return input_.variable_count();
  }

  bool floating_bound_suggests_pruning(double bound) const {
    if (incumbent_point_.empty() || !std::isfinite(bound))
      return false;
    if (objective_scale_ < -1023 || objective_scale_ > 1074)
      return false;
    const double incumbent = std::ldexp(incumbent_objective_.to_double(),
                                        static_cast<int>(-objective_scale_));
    if (!std::isfinite(incumbent))
      return false;
    const double tolerance = 1e-8 * (1.0 + std::abs(incumbent));
    return bound <= incumbent + tolerance;
  }

  search_status branch(size_t variable, bool one_first) {
    const int8_t first = one_first ? 1 : 0;
    fixed_[variable] = first;
    const search_status first_status = search();
    if (first_status != search_status::complete) {
      fixed_[variable] = -1;
      return first_status;
    }
    fixed_[variable] = static_cast<int8_t>(1 - first);
    const search_status second_status = search();
    fixed_[variable] = -1;
    return second_status;
  }

  search_status search() {
    if (result_.nodes == options_.node_limit || stopped(options_.deadline))
      return search_status::interrupted;
    ++result_.nodes;

    const std::vector<integer_row> rows = make_rows();
    const floating_lp_result floating = solve_floating(rows);
    if (floating.status == lp_status::interrupted)
      return search_status::interrupted;

    size_t branch_variable = input_.variable_count();
    if (floating.status == lp_status::optimal) {
      branch_variable = floating_branch(floating.point);
      if (branch_variable != input_.variable_count() &&
          !floating_bound_suggests_pruning(floating.objective))
        return branch(branch_variable, floating.point[branch_variable] >= 0.5);
    }

    // No floating-point status or value below this line is accepted as a
    // mathematical decision.
    exact_lp_result exact = solve_exact(rows);
    if (exact.status == lp_status::interrupted)
      return search_status::interrupted;
    if (exact.status == lp_status::infeasible)
      return search_status::complete;
    if (exact.status == lp_status::unbounded) {
      branch_variable = first_unfixed_binary();
      return branch_variable == input_.variable_count()
                 ? search_status::unbounded
                 : branch(branch_variable, true);
    }

    if (!incumbent_point_.empty() &&
        exact.objective.compare(incumbent_objective_) <= 0)
      return search_status::complete;
    branch_variable = exact_branch(exact.point);
    if (branch_variable != input_.variable_count()) {
      return branch(branch_variable,
                    exact.point[branch_variable].to_double() >= 0.5);
    }

    incumbent_objective_ = exact.objective;
    incumbent_point_ = std::move(exact.point);
    return options_.stop_on_positive_objective &&
                   incumbent_objective_.sign() > 0
               ? search_status::positive_objective_found
               : search_status::complete;
  }

  static constexpr double integrality_epsilon = 1e-8;
  const problem &input_;
  solve_options options_;
  std::vector<size_t> binary_variables_;
  std::vector<int8_t> fixed_;
  solve_result result_;
  exact_number incumbent_objective_;
  std::vector<exact_number> incumbent_point_;
  slong objective_scale_ = 0;
};

} // namespace

solve_result solver::solve(const problem &input, solve_options options) const {
  if (options.node_limit == 0 ||
      options.deadline <= std::chrono::steady_clock::now())
    return {};
  return branch_and_bound(input, options).run();
}

} // namespace coposit::fractional_milp
