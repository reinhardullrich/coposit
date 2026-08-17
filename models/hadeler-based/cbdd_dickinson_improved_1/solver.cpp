#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

class interval_cbdd {
public:
  explicit interval_cbdd(size_t dimension,
                         diagnostics::tracker *diagnostics = nullptr)
      : dimension_(dimension), diagnostics_(diagnostics),
        expiring_(dimension + 1, empty),
        current_support_(dimension) {
    nodes_.push_back({dimension_, dimension_, 0, 0}); // Constant false.
    nodes_.push_back({dimension_, dimension_, 1, 1}); // Constant true.
  }

  void start_cardinality(size_t cardinality) {
    if (!diagnostics_) {
      expire_before(cardinality);
      remaining_ = subtract(cardinality_family(cardinality), covered_);
      return;
    }
    diagnostics_->decision_diagram_phase_change(
        diagnostics::decision_diagram_phase::cardinality_build);
    expire_before(cardinality);
    remaining_ = subtract(cardinality_family(cardinality), covered_);
    publish_work();
    diagnostics_->decision_diagram_phase_change(
        diagnostics::decision_diagram_phase::support_solve);
  }

  bool take_first(std::vector<size_t> &indices) {
    if (remaining_ == empty)
      return false;

    current_support_.clear();
    size_t root = remaining_;
    while (root != unit) {
      assert(root != empty);
      const node value = nodes_[root];
      if (value.low != empty) {
        root = value.low;
      } else {
        current_support_.set(actual_index(value.bottom));
        root = value.high;
      }
    }
    current_support_.copy_indices_to(indices);
    return true;
  }

  bool add_interval_if_new(const support &lower, const support &upper,
                           size_t upper_cardinality) {
    assert(upper_cardinality >= expiration_cursor_);
    const size_t certificate = interval_family(lower, upper);
    if (diagnostics_)
      diagnostics_->decision_diagram_phase_change(
          diagnostics::decision_diagram_phase::certificate_union);
    const size_t new_covered = unite(covered_, certificate);
    if (diagnostics_)
      publish_work();
    if (new_covered == covered_) {
      if (diagnostics_)
        diagnostics_->decision_diagram_phase_change(
            diagnostics::decision_diagram_phase::support_solve);
      return false;
    }

    covered_ = new_covered;
    if (upper_cardinality < dimension_)
      expiring_[upper_cardinality] =
          unite(expiring_[upper_cardinality], certificate);
    if (diagnostics_)
      diagnostics_->decision_diagram_phase_change(
          diagnostics::decision_diagram_phase::certificate_subtract);
    remaining_ = subtract(remaining_, certificate);
    if (diagnostics_) {
      publish_work();
      diagnostics_->decision_diagram_phase_change(
          diagnostics::decision_diagram_phase::support_solve);
    }
    return true;
  }

  size_t node_count() const noexcept { return nodes_.size(); }

#ifdef COPOSIT_CBDD_DICKINSON_IMPROVED_1_TESTING
  size_t expired_bucket_count() const noexcept { return expired_bucket_count_; }
#endif

private:
  static constexpr size_t empty = 0;
  static constexpr size_t unit = 1;

  struct node {
    size_t top;
    size_t bottom;
    size_t low;
    size_t high;
  };

  struct node_key {
    size_t top;
    size_t bottom;
    size_t low;
    size_t high;

    bool operator==(const node_key &other) const noexcept {
      return top == other.top && bottom == other.bottom && low == other.low &&
             high == other.high;
    }
  };

  struct node_key_hash {
    size_t operator()(const node_key &value) const noexcept {
      size_t result = std::hash<size_t>{}(value.top);
      result ^= std::hash<size_t>{}(value.bottom) + 0x9e3779b97f4a7c15ULL +
                (result << 6) + (result >> 2);
      result ^= std::hash<size_t>{}(value.low) + 0x9e3779b97f4a7c15ULL +
                (result << 6) + (result >> 2);
      result ^= std::hash<size_t>{}(value.high) + 0x9e3779b97f4a7c15ULL +
                (result << 6) + (result >> 2);
      return result;
    }
  };

  struct pair_key {
    size_t left;
    size_t right;

    bool operator==(const pair_key &other) const noexcept {
      return left == other.left && right == other.right;
    }
  };

  struct pair_key_hash {
    size_t operator()(const pair_key &value) const noexcept {
      size_t result = std::hash<size_t>{}(value.left);
      return result ^ (std::hash<size_t>{}(value.right) +
                       0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2));
    }
  };

  size_t actual_index(size_t variable) const noexcept {
    return dimension_ - 1 - variable;
  }
  size_t top(size_t root) const noexcept {
    return root < 2 ? dimension_ : nodes_[root].top;
  }

  template <bool ReportDiagnostics> void operation_checkpoint() {
    if constexpr (ReportDiagnostics) {
      ++operations_;
      if (operations_ % diagnostics::decision_diagram_publish_interval == 0)
        publish_work();
      if ((operations_ & 4095U) == 0)
        timeout_checkpoint();
    } else {
      if ((++operations_ & 4095U) == 0)
        timeout_checkpoint();
    }
  }

  void publish_work() noexcept {
    if (diagnostics_)
      diagnostics_->decision_diagram_work(nodes_.size(), operations_);
  }

  size_t make_node(size_t top_value, size_t bottom_value, size_t low,
                   size_t high) {
    if (low == high)
      return low;

    if (low >= 2) {
      const node child = nodes_[low];
      if (child.top == bottom_value + 1 && child.high == high)
        return make_node(top_value, child.bottom, child.low, high);
    }

    const node_key key{top_value, bottom_value, low, high};
    const auto found = unique_.find(key);
    if (found != unique_.end())
      return found->second;

    const size_t result = nodes_.size();
    nodes_.push_back({top_value, bottom_value, low, high});
    unique_.emplace(key, result);
    return result;
  }

  size_t make_node(size_t variable_value, size_t low, size_t high) {
    return make_node(variable_value, variable_value, low, high);
  }

  size_t interval_family(const support &lower, const support &upper) {
    size_t root = unit;
    for (size_t variable_value = dimension_; variable_value-- > 0;) {
      const size_t bit = actual_index(variable_value);
      if (lower.contains(bit)) {
        root = make_node(variable_value, empty, root);
      } else if (!upper.contains(bit)) {
        root = make_node(variable_value, root, empty);
      }
    }
    return root;
  }

  size_t cardinality_family(size_t cardinality) {
    const size_t missing = std::numeric_limits<size_t>::max();
    std::vector<std::vector<size_t>> memo(
        dimension_ + 1, std::vector<size_t>(cardinality + 1, missing));
    std::function<size_t(size_t, size_t)> build = [&](size_t variable_value,
                                                      size_t needed) -> size_t {
      if (needed > dimension_ - variable_value)
        return empty;
      if (variable_value == dimension_)
        return needed == 0 ? unit : empty;
      size_t &cached = memo[variable_value][needed];
      if (cached != missing)
        return cached;

      const size_t low = build(variable_value + 1, needed);
      const size_t high =
          needed == 0 ? empty : build(variable_value + 1, needed - 1);
      cached = make_node(variable_value, low, high);
      return cached;
    };
    return build(0, cardinality);
  }

  void expire_before(size_t cardinality) {
    assert(cardinality >= expiration_cursor_);
    while (expiration_cursor_ < cardinality) {
      const size_t expired = expiring_[expiration_cursor_];
      if (expired != empty) {
        covered_ = subtract(covered_, expired);
        expiring_[expiration_cursor_] = empty;
#ifdef COPOSIT_CBDD_DICKINSON_IMPROVED_1_TESTING
        ++expired_bucket_count_;
#endif
      }
      ++expiration_cursor_;
    }
  }

  size_t unite(size_t left, size_t right) {
    union_cache_.clear();
    return diagnostics_ ? unite_impl<true>(left, right)
                        : unite_impl<false>(left, right);
  }

  template <bool ReportDiagnostics>
  size_t unite_impl(size_t left, size_t right) {
    operation_checkpoint<ReportDiagnostics>();
    if (left == unit || right == unit)
      return unit;
    if (left == empty || left == right)
      return right;
    if (right == empty)
      return left;
    if (right < left)
      std::swap(left, right);

    const pair_key key{left, right};
    const auto found = union_cache_.find(key);
    if (found != union_cache_.end())
      return found->second;

    const size_t top_value = std::min(top(left), top(right));
    const size_t bottom_value =
        std::min(split_bottom(left, top_value), split_bottom(right, top_value));
    const auto [left_low, left_high] = cofactors(left, bottom_value);
    const auto [right_low, right_high] = cofactors(right, bottom_value);
    const size_t result =
        make_node(top_value, bottom_value,
                  unite_impl<ReportDiagnostics>(left_low, right_low),
                  unite_impl<ReportDiagnostics>(left_high, right_high));
    union_cache_.emplace(key, result);
    return result;
  }

  size_t subtract(size_t left, size_t right) {
    difference_cache_.clear();
    return diagnostics_ ? subtract_impl<true>(left, right)
                        : subtract_impl<false>(left, right);
  }

  template <bool ReportDiagnostics>
  size_t subtract_impl(size_t left, size_t right) {
    operation_checkpoint<ReportDiagnostics>();
    if (left == empty || left == right)
      return empty;
    if (right == empty)
      return left;
    if (right == unit)
      return empty;

    const pair_key key{left, right};
    const auto found = difference_cache_.find(key);
    if (found != difference_cache_.end())
      return found->second;

    const size_t top_value = std::min(top(left), top(right));
    const size_t bottom_value =
        std::min(split_bottom(left, top_value), split_bottom(right, top_value));
    const auto [left_low, left_high] = cofactors(left, bottom_value);
    const auto [right_low, right_high] = cofactors(right, bottom_value);
    const size_t result =
        make_node(top_value, bottom_value,
                  subtract_impl<ReportDiagnostics>(left_low, right_low),
                  subtract_impl<ReportDiagnostics>(left_high, right_high));
    difference_cache_.emplace(key, result);
    return result;
  }

  size_t split_bottom(size_t root, size_t top_value) const noexcept {
    if (top(root) == top_value)
      return nodes_[root].bottom;
    if (root < 2)
      return dimension_;
    return nodes_[root].top - 1;
  }

  std::pair<size_t, size_t> cofactors(size_t root, size_t bottom_value) {
    if (bottom_value < top(root))
      return {root, root};

    const node value = nodes_[root];
    if (bottom_value == value.bottom)
      return {value.low, value.high};
    return {make_node(bottom_value + 1, value.bottom, value.low, value.high),
            value.high};
  }

  size_t dimension_;
  diagnostics::tracker *diagnostics_;
  std::vector<node> nodes_;
  std::unordered_map<node_key, size_t, node_key_hash> unique_;
  std::unordered_map<pair_key, size_t, pair_key_hash> union_cache_;
  std::unordered_map<pair_key, size_t, pair_key_hash> difference_cache_;
  std::vector<size_t> expiring_;
  size_t covered_ = empty;
  size_t remaining_ = empty;
  size_t expiration_cursor_ = 0;
#ifdef COPOSIT_CBDD_DICKINSON_IMPROVED_1_TESTING
  size_t expired_bucket_count_ = 0;
#endif
  support current_support_;
  uint64_t operations_ = 0;
};

struct pending_interval {
  explicit pending_interval(size_t dimension)
      : lower(dimension), upper(dimension) {}

  support lower;
  support upper;
  size_t lower_size = 0;
  size_t upper_size = 0;
};

class dickinson_checker {
public:
  dickinson_checker(size_t dimension, copositivity_mode mode)
      : factorization_(dimension), mode_(mode),
        diagnostics_(diagnostics::metric::decision_diagram, dimension),
        supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr) {
    indices_.reserve(dimension);
    outside_indices_.reserve(dimension);
  }

  dickinson_checker(size_t dimension,
                    copositivity_classification &classification)
      : factorization_(dimension), mode_(copositivity_mode::copositive),
        classification_(&classification),
        diagnostics_(diagnostics::metric::decision_diagram, dimension),
        supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr) {
    indices_.reserve(dimension);
    outside_indices_.reserve(dimension);
  }

  bool check(const matrix_integer &matrix) {
    for (size_t subset_dimension = 1; subset_dimension <= matrix.rows();
         ++subset_dimension) {
      diagnostics_.decision_diagram_cardinality(
          subset_dimension,
          diagnostics::decision_diagram_phase::cardinality_build);
      supports_.start_cardinality(subset_dimension);
      while (supports_.take_first(indices_)) {
        timeout_checkpoint();
        diagnostics_.decision_diagram_support();
        COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("process",
                                                      subset_dimension);
        if (!process_subset(matrix)) {
          diagnostics_.finish();
          return false;
        }
      }
    }

    diagnostics_.finish();
    return true;
  }

#ifdef COPOSIT_CBDD_DICKINSON_IMPROVED_1_TESTING
  std::vector<std::pair<uint64_t, uint64_t>>
  singular_candidates_for_test(const matrix_integer &matrix,
                               const std::vector<size_t> &indices) {
    assert(matrix.rows() <= 64);
    indices_ = indices;
    principal_.resize(indices_.size(), indices_.size());
    copy_principal(matrix, indices_, principal_);
    if (factorization_.factorize_inplace(principal_) != 0)
      return {};

    pending_.clear();
    if (!search_singular_candidates(matrix))
      return {};

    std::vector<std::pair<uint64_t, uint64_t>> result;
    result.reserve(pending_.size());
    for (const pending_interval &interval : pending_) {
      uint64_t lower = 0;
      uint64_t upper = 0;
      for (size_t bit = 0; bit < matrix.rows(); ++bit) {
        if (interval.lower.contains(bit))
          lower |= uint64_t{1} << bit;
        if (interval.upper.contains(bit))
          upper |= uint64_t{1} << bit;
      }
      result.emplace_back(lower, upper);
    }
    return result;
  }
#endif

private:
  bool process_subset(const matrix_integer &matrix) {
    const size_t dimension = indices_.size();
    principal_.resize(dimension, dimension);
    copy_principal(matrix, indices_, principal_);
    pending_.clear();

    if (factorization_.factorize_inplace(principal_) != 0) {
      solution_.resize(dimension, 1);
      for (size_t row = 0; row < dimension; ++row)
        solution_(row, 0).set_one();

      integer denominator;
      factorization_.solve_inplace(solution_, denominator, principal_);
      assert(denominator.sign() > 0);
      if (!consider_affine_candidate(matrix, solution_))
        return false;
    } else if (!search_singular_candidates(matrix)) {
      return false;
    }

    commit_pending_intervals();
    return true;
  }

  bool search_singular_candidates(const matrix_integer &matrix) {
    const size_t nullity = indices_.size() - factorization_.rank();
    assert(nullity > 0);

    if (!consider_singular_affine_companion(matrix, nullity))
      return false;

    if (nullity == 2) {
      root_basis_.resize(indices_.size(), 2);
      factorization_.nullspace_basis(root_basis_, principal_);
      build_projected_outside_rows(matrix);
      return enumerate_stacked_planar_lines();
    }

    solution_.resize(indices_.size(), 1);
    factorization_.one_nullspace_vector(solution_, principal_);
    if (nullity == 1)
      return consider_both_homogeneous_orientations(matrix, solution_);

    bool has_positive = false;
    for (size_t row = 0; row < solution_.rows(); ++row)
      has_positive |= solution_(row, 0).sign() > 0;
    if (!has_positive)
      solution_.negate();
    COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("higher-nullity-fallback",
                                                  nullity);
    return consider_homogeneous_candidate(matrix, solution_);
  }

  bool consider_singular_affine_companion(const matrix_integer &matrix,
                                          size_t nullity) {
    affine_particular_.resize(indices_.size(), 1);
    for (size_t row = 0; row < indices_.size(); ++row)
      affine_particular_(row, 0).set_one();

    integer denominator;
    if (!factorization_.solve_consistent_inplace(affine_particular_,
                                                 denominator, principal_)) {
      COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("affine-inconsistent",
                                                    nullity);
      return true;
    }
    assert(denominator.sign() > 0);
    COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("affine-consistent", nullity);
    return consider_affine_candidate(matrix, affine_particular_);
  }

  bool consider_affine_candidate(const matrix_integer &matrix,
                                 const matrix_integer &vector) {
    bool has_positive = false;
    for (size_t row = 0; row < vector.rows(); ++row)
      has_positive |= vector(row, 0).sign() > 0;
    if (!has_positive) {
      COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("negative-affine-witness",
                                                    vector.rows());
      return false;
    }

    queue_full_product_interval(matrix, vector);
    COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("affine-candidate",
                                                  support_size(vector));
    return true;
  }

  bool consider_both_homogeneous_orientations(const matrix_integer &matrix,
                                              matrix_integer &vector) {
    if (!consider_homogeneous_candidate(matrix, vector))
      return false;
    vector.negate();
    return consider_homogeneous_candidate(matrix, vector);
  }

  bool consider_homogeneous_candidate(const matrix_integer &matrix,
                                      const matrix_integer &vector) {
    bool has_positive = false;
    bool has_negative = false;
    for (size_t row = 0; row < vector.rows(); ++row) {
      has_positive |= vector(row, 0).sign() > 0;
      has_negative |= vector(row, 0).sign() < 0;
    }
    if (!has_positive)
      return true;
    if (!has_negative && !record_nonnegative_zero())
      return false;

    queue_full_product_interval(matrix, vector);
    COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("homogeneous-candidate",
                                                  support_size(vector));
    return true;
  }

  void build_projected_outside_rows(const matrix_integer &matrix) {
    outside_indices_.clear();
    size_t local = 0;
    for (size_t row = 0; row < matrix.rows(); ++row) {
      if (local < indices_.size() && indices_[local] == row) {
        ++local;
      } else {
        outside_indices_.push_back(row);
      }
    }

    projected_.resize(outside_indices_.size(), 2);
    for (size_t outside = 0; outside < outside_indices_.size(); ++outside) {
      timeout_checkpoint();
      for (size_t column = 0; column < 2; ++column) {
        projected_(outside, column).set_zero();
        for (size_t root_row = 0; root_row < indices_.size(); ++root_row)
          projected_(outside, column)
              .addmul(matrix(outside_indices_[outside], indices_[root_row]),
                      root_basis_(root_row, column));
      }
    }
  }

  bool enumerate_stacked_planar_lines() {
    seen_lines_.clear();
    coefficient_.resize(2, 1);

    for (size_t row = 0; row < root_basis_.rows(); ++row)
      if (!consider_stacked_row(root_basis_(row, 0), root_basis_(row, 1)))
        return false;
    for (size_t row = 0; row < projected_.rows(); ++row)
      if (!consider_stacked_row(projected_(row, 0), projected_(row, 1)))
        return false;
    return true;
  }

  bool consider_stacked_row(integer::const_reference first,
                            integer::const_reference second) {
    if (first.is_zero() && second.is_zero())
      return true;

    coefficient_(0, 0) = second;
    coefficient_(1, 0) = first;
    coefficient_(1, 0).negate();
    canonicalize_planar_line();
    if (!seen_lines_.insert(planar_line_key()).second) {
      COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("duplicate-stacked-line",
                                                    2);
      return true;
    }

    COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("stacked-line", 2);
    materialize_root_vector();
    if (!consider_projected_homogeneous_candidate())
      return false;
    coefficient_.negate();
    candidate_.negate();
    return consider_projected_homogeneous_candidate();
  }

  void canonicalize_planar_line() {
    integer content;
    fmpz_gcd(content.native_handle(), coefficient_(0, 0).native_handle(),
             coefficient_(1, 0).native_handle());
    assert(!content.is_zero());
    if (!content.is_one()) {
      coefficient_(0, 0).divide_exact(content);
      coefficient_(1, 0).divide_exact(content);
    }
    if (coefficient_(0, 0).sign() < 0 ||
        (coefficient_(0, 0).is_zero() && coefficient_(1, 0).sign() < 0))
      coefficient_.negate();
  }

  std::string planar_line_key() const {
    return integer(coefficient_(0, 0)).to_string() + "," +
           integer(coefficient_(1, 0)).to_string();
  }

  void materialize_root_vector() {
    candidate_.resize(root_basis_.rows(), 1);
    for (size_t row = 0; row < root_basis_.rows(); ++row) {
      candidate_(row, 0).set_zero();
      candidate_(row, 0).addmul(root_basis_(row, 0), coefficient_(0, 0));
      candidate_(row, 0).addmul(root_basis_(row, 1), coefficient_(1, 0));
    }
  }

  bool consider_projected_homogeneous_candidate() {
    bool has_positive = false;
    bool has_negative = false;
    for (size_t row = 0; row < candidate_.rows(); ++row) {
      has_positive |= candidate_(row, 0).sign() > 0;
      has_negative |= candidate_(row, 0).sign() < 0;
    }
    if (!has_positive)
      return true;
    if (!has_negative && !record_nonnegative_zero())
      return false;

    queue_projected_interval(candidate_, coefficient_);
    COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("homogeneous-candidate",
                                                  support_size(candidate_));
    return true;
  }

  bool record_nonnegative_zero() {
    if (classification_ != nullptr)
      classification_->is_strictly_copositive = false;
    else if (mode_ == copositivity_mode::strictly_copositive)
      return false;
    return true;
  }

  void queue_full_product_interval(const matrix_integer &matrix,
                                   const matrix_integer &vector) {
    pending_.emplace_back(matrix.rows());
    pending_interval &interval = pending_.back();
    fill_lower(vector, interval);

    for (size_t row = 0; row < matrix.rows(); ++row) {
      timeout_checkpoint();
      product_.set_zero();
      for (size_t local = 0; local < indices_.size(); ++local)
        product_.addmul(matrix(row, indices_[local]), vector(local, 0));
      if (product_.sign() >= 0) {
        interval.upper.set(row);
        ++interval.upper_size;
      }
    }
    assert(interval.lower.is_subset_of(interval.upper));
  }

  void queue_projected_interval(const matrix_integer &vector,
                                const matrix_integer &coefficients) {
    pending_.emplace_back(indices_.size() + outside_indices_.size());
    pending_interval &interval = pending_.back();
    fill_lower(vector, interval);

    for (const size_t index : indices_)
      interval.upper.set(index);
    interval.upper_size = indices_.size();
    for (size_t outside = 0; outside < outside_indices_.size(); ++outside) {
      timeout_checkpoint();
      product_.set_zero();
      product_.addmul(projected_(outside, 0), coefficients(0, 0));
      product_.addmul(projected_(outside, 1), coefficients(1, 0));
      if (product_.sign() >= 0) {
        interval.upper.set(outside_indices_[outside]);
        ++interval.upper_size;
      }
    }
    assert(interval.lower.is_subset_of(interval.upper));
  }

  void fill_lower(const matrix_integer &vector,
                  pending_interval &interval) const {
    for (size_t local = 0; local < indices_.size(); ++local) {
      if (vector(local, 0).is_zero())
        continue;
      interval.lower.set(indices_[local]);
      ++interval.lower_size;
    }
  }

  void commit_pending_intervals() {
    for (pending_interval &interval : pending_) {
      const size_t free_indices = interval.upper_size - interval.lower_size;
      if (!supports_.add_interval_if_new(interval.lower, interval.upper,
                                         interval.upper_size)) {
        COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("redundant-interval",
                                                      free_indices);
        continue;
      }
      diagnostics_.decision_diagram_certificate(free_indices,
                                                interval.upper_size);
      COPOSIT_CBDD_DICKINSON_IMPROVED_1_DIAGNOSTICS("retained-interval",
                                                    free_indices);
    }
    pending_.clear();
  }

  static size_t support_size(const matrix_integer &vector) {
    size_t result = 0;
    for (size_t row = 0; row < vector.rows(); ++row)
      result += !vector(row, 0).is_zero();
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

  fraction_free_ldlt_factorization factorization_;
  matrix_integer principal_;
  matrix_integer solution_;
  matrix_integer affine_particular_;
  matrix_integer root_basis_;
  matrix_integer projected_;
  matrix_integer coefficient_;
  matrix_integer candidate_;
  integer product_;
  std::vector<size_t> indices_;
  std::vector<size_t> outside_indices_;
  std::vector<pending_interval> pending_;
  std::unordered_set<std::string> seen_lines_;
  const copositivity_mode mode_;
  copositivity_classification *classification_ = nullptr;
  diagnostics::tracker diagnostics_;
  interval_cbdd supports_;
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

#ifdef COPOSIT_CBDD_DICKINSON_IMPROVED_1_TESTING
std::vector<std::pair<uint64_t, uint64_t>>
cbdd_dickinson_improved_1_singular_candidates(
    const matrix_integer &matrix, const std::vector<size_t> &indices) {
  return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
      .singular_candidates_for_test(matrix, indices);
}

size_t cbdd_dickinson_improved_1_retained_interval_count(
    size_t dimension,
    const std::vector<std::pair<uint64_t, uint64_t>> &intervals) {
  assert(dimension <= 64);
  interval_cbdd diagram(dimension);
  size_t result = 0;
  for (const auto &[lower_mask, upper_mask] : intervals) {
    support lower(dimension);
    support upper(dimension);
    size_t upper_size = 0;
    for (size_t bit = 0; bit < dimension; ++bit) {
      if ((lower_mask & (uint64_t{1} << bit)) != 0)
        lower.set(bit);
      if ((upper_mask & (uint64_t{1} << bit)) != 0) {
        upper.set(bit);
        ++upper_size;
      }
    }
    result += diagram.add_interval_if_new(lower, upper, upper_size);
  }
  return result;
}

std::pair<size_t, size_t> cbdd_dickinson_improved_1_expiry_result(
    size_t dimension, size_t cardinality,
    const std::vector<std::pair<uint64_t, uint64_t>> &intervals) {
  interval_cbdd diagram(dimension);
  for (const auto &[lower_mask, upper_mask] : intervals) {
    support lower(dimension);
    support upper(dimension);
    size_t upper_size = 0;
    for (size_t bit = 0; bit < dimension; ++bit) {
      if ((lower_mask & (uint64_t{1} << bit)) != 0)
        lower.set(bit);
      if ((upper_mask & (uint64_t{1} << bit)) != 0) {
        upper.set(bit);
        ++upper_size;
      }
    }
    diagram.add_interval_if_new(lower, upper, upper_size);
  }
  diagram.start_cardinality(cardinality);
  std::vector<size_t> indices;
  size_t uncovered = 0;
  while (diagram.take_first(indices)) {
    support exact(dimension);
    for (const size_t index : indices)
      exact.set(index);
    diagram.add_interval_if_new(exact, exact, indices.size());
    ++uncovered;
  }
  return {uncovered, diagram.expired_bucket_count()};
}
#endif

} // namespace coposit::model
