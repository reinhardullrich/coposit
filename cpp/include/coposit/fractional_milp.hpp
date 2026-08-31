#pragma once

#include <coposit/integer.hpp>

#include <chrono>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace coposit::fractional_milp {

enum class variable_type { continuous, binary };
enum class constraint_sense { less_equal, equal, greater_equal };
enum class solve_status {
  optimal,
  positive_objective_found,
  infeasible,
  unbounded,
  interrupted
};

// Canonical exact output value. The denominator is always positive and the
// fraction is reduced.
class fraction {
public:
  fraction();
  explicit fraction(slong value);
  fraction(integer numerator, integer denominator);

  const integer &numerator() const noexcept { return numerator_; }
  const integer &denominator() const noexcept { return denominator_; }
  int sign() const noexcept { return numerator_.sign(); }
  double to_double() const noexcept;
  std::string to_string() const;

private:
  integer numerator_;
  integer denominator_{1};
};

class problem {
public:
  struct linear_constraint {
    std::vector<integer> coefficients;
    constraint_sense sense;
    integer right_hand_side;
  };

  explicit problem(size_t variable_count);

  size_t variable_count() const noexcept { return variable_types_.size(); }
  variable_type type(size_t variable) const {
    return variable_types_.at(variable);
  }
  bool has_upper_bound(size_t variable) const {
    return has_upper_bound_.at(variable);
  }
  const integer &upper_bound(size_t variable) const {
    return upper_bounds_.at(variable);
  }
  const std::vector<integer> &objective() const noexcept { return objective_; }
  const std::vector<linear_constraint> &constraints() const noexcept {
    return constraints_;
  }
  void set_variable_type(size_t variable, variable_type type);
  void set_upper_bound(size_t variable, integer upper_bound);
  void set_objective_coefficient(size_t variable, integer coefficient);
  void add_constraint(std::vector<integer> coefficients, constraint_sense sense,
                      integer right_hand_side);

private:
  std::vector<variable_type> variable_types_;
  std::vector<integer> upper_bounds_;
  std::vector<bool> has_upper_bound_;
  std::vector<integer> objective_;
  std::vector<linear_constraint> constraints_;
};

struct solve_options {
  size_t node_limit = std::numeric_limits<size_t>::max();
  std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::time_point::max();
  bool stop_on_positive_objective = false;
};

struct solve_result {
  solve_status status = solve_status::interrupted;
  std::vector<fraction> point;
  fraction objective;
  size_t nodes = 0;
  size_t floating_lp_solves = 0;
  size_t exact_lp_solves = 0;
};

/*
 * Floating-point LP relaxations are advisory only. Every incumbent,
 * infeasibility decision, unboundedness decision, and node bound used for
 * pruning is recomputed with FLINT rational arithmetic. If the search cannot
 * finish, the result is interrupted rather than inferred from a floating-point
 * tolerance.
 */
class solver {
public:
  solve_result solve(const problem &input, solve_options options = {}) const;
};

} // namespace coposit::fractional_milp
