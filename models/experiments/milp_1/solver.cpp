#include <coposit/diagnostics.hpp>
#include <coposit/fractional_milp.hpp>
#include <coposit/model.hpp>
#include <coposit/timeout.hpp>

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace coposit::model {

namespace {

using fractional_milp::constraint_sense;
using fractional_milp::problem;
using fractional_milp::solve_result;
using fractional_milp::solve_status;
using fractional_milp::solver;
using fractional_milp::variable_type;

size_t x_index(size_t index) noexcept { return index; }
size_t y_index(size_t dimension, size_t index) noexcept {
  return dimension + index;
}
size_t z_index(size_t dimension, size_t index) noexcept {
  return 2 * dimension + index;
}

integer minimum_entry(const matrix_integer &matrix) {
  integer result(matrix(0, 0));
  for (size_t row = 0; row < matrix.rows(); ++row)
    for (size_t column = 0; column < matrix.cols(); ++column)
      if (matrix(row, column).compare(result) < 0)
        result = matrix(row, column);
  return result;
}

integer minimum_diagonal(const matrix_integer &matrix) {
  integer result(matrix(0, 0));
  for (size_t index = 1; index < matrix.rows(); ++index)
    if (matrix(index, index).compare(result) < 0)
      result = matrix(index, index);
  return result;
}

problem make_stqp_problem(const matrix_integer &matrix,
                          const integer &lower_bound) {
  const size_t dimension = matrix.rows();
  const size_t shifted_alpha = 3 * dimension;
  problem input(shifted_alpha + 1);
  input.set_objective_coefficient(shifted_alpha, integer(-1));

  integer shifted_alpha_upper;
  shifted_alpha_upper.set_difference(minimum_diagonal(matrix), lower_bound);
  input.set_upper_bound(shifted_alpha, std::move(shifted_alpha_upper));

  std::vector<integer> normalization(input.variable_count());
  for (size_t index = 0; index < dimension; ++index) {
    normalization[x_index(index)] = integer(1);
    input.set_variable_type(y_index(dimension, index), variable_type::binary);
  }
  input.add_constraint(std::move(normalization), constraint_sense::equal,
                       integer(1));

  for (size_t row = 0; row < dimension; ++row) {
    integer slack_bound(matrix(row, 0));
    for (size_t column = 1; column < dimension; ++column)
      if (matrix(row, column).compare(slack_bound) > 0)
        slack_bound = matrix(row, column);
    slack_bound -= lower_bound;

    std::vector<integer> payoff(input.variable_count());
    for (size_t column = 0; column < dimension; ++column)
      payoff[x_index(column)] = matrix(row, column);
    payoff[z_index(dimension, row)] = integer(-1);
    payoff[shifted_alpha] = integer(-1);
    input.add_constraint(std::move(payoff), constraint_sense::less_equal,
                         integer(lower_bound));

    std::vector<integer> support_link(input.variable_count());
    support_link[x_index(row)] = integer(1);
    support_link[y_index(dimension, row)] = integer(-1);
    input.add_constraint(std::move(support_link), constraint_sense::less_equal,
                         integer(0));

    std::vector<integer> slack_link(input.variable_count());
    slack_link[z_index(dimension, row)] = integer(1);
    slack_link[y_index(dimension, row)] = slack_bound;
    input.add_constraint(std::move(slack_link), constraint_sense::less_equal,
                         std::move(slack_bound));
  }
  return input;
}

const char *status_name(solve_status status) noexcept {
  switch (status) {
  case solve_status::optimal:
    return "optimal";
  case solve_status::positive_objective_found:
    return "positive_objective_found";
  case solve_status::infeasible:
    return "infeasible";
  case solve_status::unbounded:
    return "unbounded";
  case solve_status::interrupted:
    return "interrupted";
  }
  return "unknown";
}

integer minimum_numerator(const integer &lower_bound,
                          const solve_result &result) {
  integer numerator;
  numerator.set_product(lower_bound, result.objective.denominator());
  numerator -= result.objective.numerator();
  return numerator;
}

void record_result(const solve_result &result, const integer &lower_bound) {
  std::ostringstream event;
  event << "phase=stqp_milp2 status=" << status_name(result.status)
        << " nodes=" << result.nodes
        << " floating_lp_solves=" << result.floating_lp_solves
        << " exact_lp_solves=" << result.exact_lp_solves;
  if (result.status == solve_status::optimal) {
    event << " transformed_objective=" << result.objective.to_string()
          << " simplex_minimum_numerator="
          << minimum_numerator(lower_bound, result).to_string()
          << " simplex_minimum_denominator="
          << result.objective.denominator().to_string();
  }
  diagnostics::record_history_event("milp", event.str());
}

copositivity_classification classify_exact(const matrix_integer &matrix) {
  diagnostics::tracker diagnostics(diagnostics::metric::traversal, 1);
  diagnostics.stage(1);

  const integer lower_bound = minimum_entry(matrix);
  const solve_result result =
      solver().solve(make_stqp_problem(matrix, lower_bound));
  record_result(result, lower_bound);
  if (result.status == solve_status::interrupted) {
    timeout_checkpoint();
    throw std::runtime_error("fractional MILP search was interrupted");
  }
  if (result.status != solve_status::optimal)
    throw std::logic_error(
        "Gondzio-Yildirim MILP2 returned an invalid outcome");

  const integer numerator = minimum_numerator(lower_bound, result);
  diagnostics.finish();
  return {numerator.sign() >= 0, numerator.sign() > 0};
}

} // namespace

bool solve(const matrix_integer &matrix, copositivity_mode mode) {
  const copositivity_classification result = classify_exact(matrix);
  return mode == copositivity_mode::strictly_copositive
             ? result.is_strictly_copositive
             : result.is_copositive;
}

copositivity_classification classify(const matrix_integer &matrix) {
  return classify_exact(matrix);
}

} // namespace coposit::model
