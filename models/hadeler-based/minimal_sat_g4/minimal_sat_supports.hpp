#pragma once

#include <coposit/minimal_sat.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit {

class minimal_sat_supports {
public:
  explicit minimal_sat_supports(support_context &context)
      : context_(context), dimension_(context.dimension()), sat_(dimension_) {
    low_.unexplored.reserve(dimension_ + 1);
    high_.unexplored.reserve(dimension_ + 1);
    assumptions_.reserve(dimension_);
  }

  ~minimal_sat_supports() {
    clear_stream(low_);
    clear_stream(high_);
    for (interval &value : intervals_) {
      context_.release(std::move(value.lower));
      context_.release(std::move(value.upper));
    }
  }

  minimal_sat_supports(const minimal_sat_supports &) = delete;
  minimal_sat_supports &operator=(const minimal_sat_supports &) = delete;

  void add_interval(const support &lower, const support &upper) {
    if (!context_.is_subset_of(lower, upper))
      throw std::invalid_argument("invalid Minimal SAT support interval");
    if (all_covered_)
      return;

    std::vector<int> clause;
    clause.reserve(context_.count(lower) + dimension_ - context_.count(upper));
    for (size_t index = 0; index < dimension_; ++index) {
      if (context_.contains(lower, index))
        clause.push_back(-static_cast<int>(index + 1));
      else if (!context_.contains(upper, index))
        clause.push_back(static_cast<int>(index + 1));
    }

    support stored_lower = context_.clone(lower);
    support stored_upper = [&] {
      try {
        return context_.clone(upper);
      } catch (...) {
        context_.release(std::move(stored_lower));
        throw;
      }
    }();
    try {
      sat_.add_clause(std::move(clause));
      intervals_.push_back({std::move(stored_lower), std::move(stored_upper)});
    } catch (...) {
      context_.release(std::move(stored_lower));
      context_.release(std::move(stored_upper));
      throw;
    }

    if (context_.empty(lower) && context_.count(upper) == dimension_) {
      all_covered_ = true;
      exhaust(low_);
      exhaust(high_);
    }
  }

  void start_low_cardinality(size_t cardinality) { start(low_, cardinality); }
  void start_high_cardinality(size_t cardinality) { start(high_, cardinality); }
  bool take_first_low(std::vector<size_t> &indices) {
    return take_first(low_, indices);
  }
  bool take_first_high(std::vector<size_t> &indices) {
    return take_first(high_, indices);
  }

  size_t interval_count() const noexcept { return intervals_.size(); }

  bool covers(const support &candidate) const {
    for (const interval &value : intervals_)
      if (context_.is_subset_of(value.lower, candidate) &&
          context_.is_subset_of(candidate, value.upper))
        return true;
    return false;
  }

  bool covers_interval(const support &lower, const support &upper) const {
    if (!context_.is_subset_of(lower, upper))
      throw std::invalid_argument("invalid Minimal SAT support interval");
    for (const interval &value : intervals_)
      if (context_.is_subset_of(value.lower, lower) &&
          context_.is_subset_of(upper, value.upper))
        return true;
    return false;
  }

private:
  struct interval {
    support lower;
    support upper;
  };

  struct prefix {
    support values;
    size_t length;
  };

  struct stream {
    bool started = false;
    bool exhausted = false;
    size_t cardinality = 0;
    std::vector<prefix> unexplored;
  };

  void start(stream &selected, size_t cardinality) {
    if (cardinality > dimension_)
      throw std::invalid_argument("support cardinality exceeds the dimension");
    if (selected.started && selected.cardinality == cardinality)
      return;
    clear_stream(selected);
    selected.started = true;
    selected.cardinality = cardinality;
    selected.exhausted = all_covered_;
    if (!selected.exhausted)
      selected.unexplored.push_back({context_.make(), 0});
  }

  bool take_first(stream &selected, std::vector<size_t> &indices) {
    if (!selected.started)
      throw std::logic_error("support cardinality stream was not started");
    while (!selected.exhausted && !selected.unexplored.empty()) {
      timeout_checkpoint();
      prefix current = std::move(selected.unexplored.back());
      selected.unexplored.pop_back();

      assumptions_.clear();
      for (size_t index = 0; index < current.length; ++index)
        assumptions_.push_back(context_.contains(current.values, index)
                                   ? static_cast<int>(index + 1)
                                   : -static_cast<int>(index + 1));
      if (!sat_.solve_exactly(selected.cardinality, assumptions_)) {
        context_.release(std::move(current.values));
        continue;
      }

      support model = context_.make();
      for (size_t index = 0; index < dimension_; ++index)
        if (sat_.value(index))
          context_.set(model, index);
      selected.unexplored.reserve(selected.unexplored.size() + dimension_ -
                                  current.length);
      for (size_t index = current.length; index < dimension_; ++index) {
        support sibling = context_.clone(model);
        if (context_.contains(sibling, index))
          context_.reset(sibling, index);
        else
          context_.set(sibling, index);
        selected.unexplored.push_back({std::move(sibling), index + 1});
      }

      context_.extract_set_indices(model, indices);
      context_.release(std::move(model));
      context_.release(std::move(current.values));
      return true;
    }
    selected.exhausted = true;
    return false;
  }

  void exhaust(stream &selected) noexcept {
    for (prefix &value : selected.unexplored)
      context_.release(std::move(value.values));
    selected.unexplored.clear();
    selected.exhausted = true;
  }

  void clear_stream(stream &selected) noexcept {
    exhaust(selected);
    selected.started = false;
    selected.exhausted = false;
    selected.cardinality = 0;
  }

  support_context &context_;
  size_t dimension_;
  minimal_sat sat_;
  std::vector<interval> intervals_;
  std::vector<int> assumptions_;
  stream low_;
  stream high_;
  bool all_covered_ = false;
};

} // namespace coposit
