#include <coposit/minimal_sat.hpp>
#include <coposit/timeout.hpp>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace coposit {

minimal_sat::minimal_sat(size_t variable_count)
    : variable_count_(variable_count), assignments_(variable_count, int8_t{-1}),
      required_true_count_(variable_count + 1) {
  if (variable_count > static_cast<size_t>(std::numeric_limits<int>::max()))
    throw std::invalid_argument("too many SAT variables");
  trail_.reserve(variable_count);
}

void minimal_sat::add_clause(std::vector<int> clause) {
  std::vector<int> normalized;
  normalized.reserve(clause.size());
  for (const int literal : clause) {
    if (literal == 0 || literal == std::numeric_limits<int>::min() ||
        static_cast<size_t>(std::abs(literal)) > variable_count_)
      throw std::invalid_argument("SAT literal is outside the variable range");
    if (std::find(normalized.begin(), normalized.end(), literal) !=
        normalized.end())
      continue;
    if (std::find(normalized.begin(), normalized.end(), -literal) !=
        normalized.end())
      return;
    normalized.push_back(literal);
  }
  clauses_.push_back(std::move(normalized));
  has_model_ = false;
}

bool minimal_sat::solve() { return solve_with_limit(variable_count_ + 1, {}); }

bool minimal_sat::solve_exactly(size_t true_count,
                                const std::vector<int> &assumptions) {
  if (true_count > variable_count_)
    throw std::invalid_argument("SAT true count exceeds the variable count");
  return solve_with_limit(true_count, assumptions);
}

bool minimal_sat::solve_with_limit(size_t true_count,
                                   const std::vector<int> &assumptions) {
  std::fill(assignments_.begin(), assignments_.end(), int8_t{-1});
  trail_.clear();
  required_true_count_ = true_count;
  has_model_ = true;
  for (const int literal : assumptions) {
    if (literal == 0 || literal == std::numeric_limits<int>::min() ||
        static_cast<size_t>(std::abs(literal)) > variable_count_)
      throw std::invalid_argument(
          "SAT assumption is outside the variable range");
    if (!assign(literal)) {
      has_model_ = false;
      break;
    }
  }
  if (has_model_)
    has_model_ = search();
  if (!has_model_)
    undo(0);
  return has_model_;
}

bool minimal_sat::value(size_t variable) const {
  if (variable >= variable_count_)
    throw std::out_of_range("SAT variable is outside the variable range");
  if (!has_model_)
    throw std::logic_error("SAT solver has no model");
  return assignments_[variable] != 0;
}

bool minimal_sat::assign(int literal) {
  const size_t variable = static_cast<size_t>(std::abs(literal) - 1);
  const int8_t requested = literal > 0 ? int8_t{1} : int8_t{0};
  if (assignments_[variable] >= 0)
    return assignments_[variable] == requested;
  assignments_[variable] = requested;
  trail_.push_back(variable);
  return true;
}

bool minimal_sat::propagate() {
  bool changed;
  do {
    changed = false;
    if (required_true_count_ <= variable_count_) {
      size_t selected = 0;
      size_t unassigned = 0;
      for (const int8_t assignment : assignments_) {
        selected += static_cast<size_t>(assignment > 0);
        unassigned += static_cast<size_t>(assignment < 0);
      }
      if (selected > required_true_count_ ||
          selected + unassigned < required_true_count_)
        return false;
      if (unassigned != 0 && (selected == required_true_count_ ||
                              selected + unassigned == required_true_count_)) {
        const bool force_true = selected + unassigned == required_true_count_;
        for (size_t variable = 0; variable < variable_count_; ++variable) {
          if (assignments_[variable] >= 0)
            continue;
          if (!assign(force_true ? static_cast<int>(variable + 1)
                                 : -static_cast<int>(variable + 1)))
            return false;
        }
        changed = true;
      }
    }
    for (const std::vector<int> &clause : clauses_) {
      timeout_checkpoint();
      bool satisfied = false;
      size_t unassigned = 0;
      int unit = 0;
      for (const int literal : clause) {
        const int8_t assignment =
            assignments_[static_cast<size_t>(std::abs(literal) - 1)];
        if (assignment < 0) {
          ++unassigned;
          unit = literal;
        } else if ((assignment != 0) == (literal > 0)) {
          satisfied = true;
          break;
        }
      }
      if (satisfied)
        continue;
      if (unassigned == 0)
        return false;
      if (unassigned == 1) {
        if (!assign(unit))
          return false;
        changed = true;
      }
    }
  } while (changed);
  return true;
}

bool minimal_sat::search() {
  timeout_checkpoint();
  if (!propagate())
    return false;

  const auto next =
      std::find(assignments_.begin(), assignments_.end(), int8_t{-1});
  if (next == assignments_.end())
    return true;

  const size_t variable = static_cast<size_t>(next - assignments_.begin());
  const size_t trail_size = trail_.size();
  if (assign(static_cast<int>(variable + 1)) && search())
    return true;
  undo(trail_size);
  if (assign(-static_cast<int>(variable + 1)) && search())
    return true;
  undo(trail_size);
  return false;
}

void minimal_sat::undo(size_t trail_size) noexcept {
  while (trail_.size() > trail_size) {
    assignments_[trail_.back()] = -1;
    trail_.pop_back();
  }
}

} // namespace coposit
