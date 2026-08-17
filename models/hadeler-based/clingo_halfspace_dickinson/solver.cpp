#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/timeout.hpp>

#include <clingo.hh>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <limits>
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

#ifdef COPOSIT_CLINGO_HALFSPACE_DICKINSON_TESTING
size_t last_optimized_certificate_count = 0;
#endif

constexpr char selection_program[] = R"(
{ selected(0..n-1) }.
#external expired(0..n).
)";
constexpr char cardinality_program[] = R"(
#external active(k).
:- active(k), #count { Index : selected(Index) } != k.
)";

struct coverage_score {
  size_t width = 0;
  size_t upper_size = 0;
};

bool better(const coverage_score &candidate,
            const coverage_score &current) noexcept {
  return candidate.width > current.width ||
         (candidate.width == current.width &&
          candidate.upper_size > current.upper_size);
}

class dickinson_checker final : public Clingo::SolveEventHandler {
public:
  dickinson_checker(size_t dimension, copositivity_mode mode)
      : factorization_(dimension), product_(dimension), mode_(mode),
        diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
    selected_literals_.reserve(dimension);
    expiration_literals_.reserve(dimension + 1);
    clause_.reserve(dimension);
  }

  dickinson_checker(size_t dimension,
                    copositivity_classification &classification)
      : factorization_(dimension), product_(dimension),
        mode_(copositivity_mode::copositive), classification_(&classification),
        diagnostics_(diagnostics::metric::support, dimension) {
    indices_.reserve(dimension);
    selected_literals_.reserve(dimension);
    expiration_literals_.reserve(dimension + 1);
    clause_.reserve(dimension);
  }

  bool check(const matrix_integer &matrix) {
#ifdef COPOSIT_CLINGO_HALFSPACE_DICKINSON_TESTING
    optimized_certificate_count_ = 0;
#endif
    if (matrix.rows() > static_cast<size_t>(std::numeric_limits<int>::max()))
      throw std::overflow_error(
          "clingo supports at most INT_MAX matrix indices");

    matrix_ = &matrix;
    const std::array<char const *, 4> arguments{
        "--parallel-mode=1", "--enum-mode=bt", "--models=0", "--warn=none"};
    Clingo::Control control(arguments);
    control.add("base", {"n"}, selection_program);
    control.add("cardinality", {"k"}, cardinality_program);
    const Clingo::Symbol dimension =
        Clingo::Number(static_cast<int>(matrix.rows()));
    control.ground({Clingo::Part{"base", {dimension}}});
    const Clingo::SymbolicAtoms atoms = control.symbolic_atoms();
    for (size_t index = 0; index < matrix.rows(); ++index) {
      const Clingo::Symbol atom = Clingo::Function(
          "selected", {Clingo::Number(static_cast<int>(index))});
      auto iterator = atoms.find(atom);
      if (!iterator)
        throw std::runtime_error(
            "clingo did not ground a selected/1 support atom");
      selected_literals_.push_back(iterator->literal());
    }
    for (size_t upper_size = 0; upper_size <= matrix.rows(); ++upper_size) {
      const Clingo::Symbol atom = Clingo::Function(
          "expired", {Clingo::Number(static_cast<int>(upper_size))});
      auto iterator = atoms.find(atom);
      if (!iterator)
        throw std::runtime_error(
            "clingo did not ground an expired/1 cardinality atom");
      expiration_literals_.push_back(iterator->literal());
      control.assign_external(iterator->literal(), Clingo::TruthValue::False);
    }

    for (size_t subset_dimension = 1; subset_dimension <= matrix.rows();
         ++subset_dimension) {
      timeout_checkpoint();
      diagnostics_.stage(subset_dimension);
      current_cardinality_ = subset_dimension;
      if (subset_dimension > 1)
        control.assign_external(expiration_literals_[subset_dimension - 1],
                                Clingo::TruthValue::True);
      const Clingo::Symbol cardinality =
          Clingo::Number(static_cast<int>(subset_dimension));
      control.ground({Clingo::Part{"cardinality", {cardinality}}});
      const Clingo::Symbol active = Clingo::Function("active", {cardinality});
      auto active_atom = control.symbolic_atoms().find(active);
      if (!active_atom)
        throw std::runtime_error(
            "clingo did not ground the active cardinality atom");
      const Clingo::literal_t active_literal = active_atom->literal();
      control.assign_external(active_literal, Clingo::TruthValue::True);

      auto handle =
          control.solve(Clingo::SymbolicLiteralSpan{}, this, true, false);
#ifdef COPOSIT_ENABLE_TIMEOUTS
      while (!handle.wait(0.05)) {
        if (!timeout_pending())
          continue;
        timed_out_ = true;
        handle.cancel();
        break;
      }
#else
      handle.wait();
#endif
      const Clingo::SolveResult result = handle.get();
      handle.close();
      control.assign_external(active_literal, Clingo::TruthValue::False);

      if (timed_out_)
        throw timeout_requested{};
      if (failed_) {
        diagnostics_.finish();
#ifdef COPOSIT_CLINGO_HALFSPACE_DICKINSON_TESTING
        last_optimized_certificate_count = optimized_certificate_count_;
#endif
        return false;
      }
      if (result.is_interrupted()) {
        timeout_checkpoint();
        throw std::runtime_error("clasp interrupted without a coposit timeout");
      }
      if (!result.is_exhausted())
        throw std::runtime_error(
            "clasp stopped before exhausting the active cardinality");
      install_pending_clauses(control);
    }

    diagnostics_.finish();
#ifdef COPOSIT_CLINGO_HALFSPACE_DICKINSON_TESTING
    last_optimized_certificate_count = optimized_certificate_count_;
#endif
    return true;
  }

private:
  bool on_model(Clingo::Model &model) override {
    timeout_checkpoint();
    indices_.clear();
    for (size_t index = 0; index < selected_literals_.size(); ++index)
      if (model.is_true(selected_literals_[index]))
        indices_.push_back(index);
    if (indices_.size() != current_cardinality_)
      throw std::runtime_error(
          "clasp produced a completed support outside the active cardinality");

    diagnostics_.visit_support();
    diagnostics_.secondary();
    COPOSIT_CLINGO_HALFSPACE_DIAGNOSTICS("process", current_cardinality_);
    Clingo::SolveControl control = model.context();
    if (!process_subset(*matrix_, control)) {
      failed_ = true;
      control.add_clause(Clingo::LiteralSpan{});
      return false;
    }
    return true;
  }

  bool process_subset(const matrix_integer &matrix,
                      Clingo::SolveControl &control) {
    const size_t dimension = indices_.size();
    principal_.resize(dimension, dimension);
    solution_.resize(dimension, 1);
    copy_principal(matrix, indices_, principal_);

    const bool singular = factorization_.factorize_inplace(principal_) == 0;
    if (singular)
      return process_singular_subset(matrix, control);
    return process_nonsingular_subset(matrix, control);
  }

  bool process_singular_subset(const matrix_integer &matrix,
                               Clingo::SolveControl &control) {
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
    return add_certificate(matrix, control);
  }

  bool process_nonsingular_subset(const matrix_integer &matrix,
                                  Clingo::SolveControl &control) {
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
        for (size_t direction = 0; direction < dimension; ++direction) {
          if (first_pass)
            calculate_nonsingular_product(matrix, directions_, direction,
                                          direction_denominator,
                                          direction_products_, direction);
          bool improved = false;
          if (!optimize_direction(direction, improved))
            return false;
          pass_improved |= improved;
          if (current_score_.width + 1 == matrix.rows())
            break;
        }
        first_pass = false;
      } while (pass_improved && current_score_.width + 1 < matrix.rows());
    }

    return add_certificate(matrix, control);
  }

  bool optimize_direction(size_t direction, bool &improved) {
    improved = false;
    best_score_ = current_score_;
    best_numerator_.set_zero();
    best_denominator_.set_one();
    negative_witness_found_ = false;

    find_breakpoints(direction);
    if (breakpoint_events_.empty())
      return true;
    sweep_direction(direction);
    if (negative_witness_found_)
      return false;
    if (best_numerator_.is_zero())
      return true;

    apply_candidate(direction, best_numerator_, best_denominator_);
    current_score_ = best_score_;
    improved = true;
#ifdef COPOSIT_CLINGO_HALFSPACE_DICKINSON_TESTING
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
    for (size_t row = 0; row < product_.size(); ++row) {
      const int base_sign = product_[row].sign();
      const int direction_sign = direction_products_(row, direction).sign();
      interval_upper_size +=
          base_sign > 0 || (base_sign == 0 && direction_sign >= 0);
    }

    positive_ratio sample;
    sample.numerator = breakpoint_events_.front().root.numerator;
    sample.denominator = breakpoint_events_.front().root.denominator;
    sample.denominator.multiply(2);
    consider_signature(interval_lower_size, interval_upper_size,
                       interval_positive_size, sample);

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
        } else {
          ++negative_product_event_count;
        }
      }

      consider_signature(root_lower_size, root_upper_size, root_positive_size,
                         breakpoint_events_[group_begin].root);
      if (negative_witness_found_)
        return;
      interval_lower_size = root_lower_size + solution_event_count;
      interval_upper_size = root_upper_size - negative_product_event_count;
      interval_positive_size =
          root_positive_size + positive_solution_event_count;

      if (group_end < breakpoint_events_.size())
        midpoint(sample, breakpoint_events_[group_begin].root,
                 breakpoint_events_[group_end].root);
      else {
        sample.numerator = breakpoint_events_[group_begin].root.numerator;
        sample.numerator += breakpoint_events_[group_begin].root.denominator;
        sample.denominator = breakpoint_events_[group_begin].root.denominator;
      }
      consider_signature(interval_lower_size, interval_upper_size,
                         interval_positive_size, sample);
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
                          size_t positive_size,
                          const positive_ratio &candidate) {
    if (positive_size == 0) {
      negative_witness_found_ = true;
      return;
    }
    assert(upper_size >= lower_size);
    const coverage_score candidate_score{upper_size - lower_size, upper_size};
    if (!better(candidate_score, best_score_))
      return;
    best_score_ = candidate_score;
    best_numerator_ = candidate.numerator;
    best_denominator_ = candidate.denominator;
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

  bool add_certificate(const matrix_integer &matrix,
                       Clingo::SolveControl &control) {
    clause_.clear();
    size_t lower_size = 0;
    size_t upper_size = 0;
    for (size_t local = 0; local < indices_.size(); ++local) {
      if (!solution_(local, 0).is_zero()) {
        clause_.push_back(-selected_literals_[indices_[local]]);
        ++lower_size;
      }
    }
    for (size_t row = 0; row < product_.size(); ++row) {
      if (product_[row].sign() >= 0) {
        ++upper_size;
      } else {
        clause_.push_back(selected_literals_[row]);
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

    assert(!clause_.empty());
    assert(upper_size >= current_cardinality_);
    control.add_clause(Clingo::LiteralSpan{clause_});
    if (upper_size > current_cardinality_) {
      for (const Clingo::literal_t literal : clause_)
        pending_clause_bodies_.push_back(-literal);
      if (upper_size < matrix.rows()) {
        pending_clause_bodies_.push_back(-expiration_literals_[upper_size]);
        COPOSIT_CLINGO_HALFSPACE_DIAGNOSTICS("expiry_guard", upper_size);
      }
      pending_clause_ends_.push_back(pending_clause_bodies_.size());
    } else {
      COPOSIT_CLINGO_HALFSPACE_DIAGNOSTICS("current_layer_only", upper_size);
    }
    if (diagnostics_.active())
      diagnostics_.certificate(upper_size - lower_size, upper_size);
    return true;
  }

  void install_pending_clauses(Clingo::Control &control) {
    if (pending_clause_ends_.empty())
      return;
    auto backend = control.backend();
    size_t begin = 0;
    for (const size_t end : pending_clause_ends_) {
      backend.rule(false, {},
                   Clingo::LiteralSpan{pending_clause_bodies_.data() + begin,
                                       end - begin});
      begin = end;
    }
    pending_clause_bodies_.clear();
    pending_clause_ends_.clear();
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

  fraction_free_ldlt_factorization factorization_;
  matrix_integer principal_;
  matrix_integer solution_;
  matrix_integer directions_;
  matrix_integer direction_products_;
  std::vector<integer> product_;
  std::vector<size_t> indices_;
  std::vector<breakpoint_event> breakpoint_events_;
  std::vector<Clingo::literal_t> selected_literals_;
  std::vector<Clingo::literal_t> expiration_literals_;
  std::vector<Clingo::literal_t> clause_;
  std::vector<Clingo::literal_t> pending_clause_bodies_;
  std::vector<size_t> pending_clause_ends_;
  coverage_score current_score_;
  coverage_score best_score_;
  integer best_numerator_;
  integer best_denominator_;
  integer scratch_;
  bool negative_witness_found_ = false;
  const copositivity_mode mode_;
  copositivity_classification *classification_ = nullptr;
  diagnostics::tracker diagnostics_;
  const matrix_integer *matrix_ = nullptr;
  size_t current_cardinality_ = 0;
  bool failed_ = false;
  bool timed_out_ = false;
#ifdef COPOSIT_CLINGO_HALFSPACE_DICKINSON_TESTING
  size_t optimized_certificate_count_ = 0;
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

#ifdef COPOSIT_CLINGO_HALFSPACE_DICKINSON_TESTING
bool clingo_halfspace_prefers_negative_singular_orientation_for_testing(
    size_t positive_products, size_t negative_products) noexcept {
  return negative_orientation_has_larger_upper(positive_products,
                                                negative_products);
}

size_t clingo_halfspace_optimized_certificate_count_for_testing() noexcept {
  return last_optimized_certificate_count;
}
#endif

} // namespace coposit::model
