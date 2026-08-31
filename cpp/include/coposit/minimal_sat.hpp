#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coposit {

/** Minimal DPLL SAT solver with unit propagation and no clause learning.
 * Literals are numbered +/-1, +/-2, ... */
class minimal_sat {
public:
  explicit minimal_sat(size_t variable_count);

  void add_clause(std::vector<int> clause);
  bool solve();
  bool solve_exactly(size_t true_count, const std::vector<int> &assumptions);
  bool value(size_t variable) const;

private:
  bool assign(int literal);
  bool propagate();
  bool search();
  bool solve_with_limit(size_t true_count, const std::vector<int> &assumptions);
  void undo(size_t trail_size) noexcept;

  size_t variable_count_;
  std::vector<std::vector<int>> clauses_;
  std::vector<int8_t> assignments_;
  std::vector<size_t> trail_;
  size_t required_true_count_;
  bool has_model_ = false;
};

} // namespace coposit
