#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/progress.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_trace.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

class interval_zdd {
public:
    explicit interval_zdd(size_t dimension)
        : dimension_(dimension)
        , current_support_(dimension)
    {
        nodes_.push_back({dimension_, 0, 0}); // Empty family.
        nodes_.push_back({dimension_, 1, 1}); // Family containing only the empty support.
    }

    void start_cardinality(size_t cardinality)
    {
        remaining_ = subtract(cardinality_family(cardinality), covered_);
    }

    bool take_first(std::vector<size_t>& indices)
    {
        if (remaining_ == empty) return false;

        current_support_.clear();
        size_t root = remaining_;
        while (root != unit) {
            assert(root != empty);
            const node value = nodes_[root];
            if (value.low != empty) {
                root = value.low;
            } else {
                current_support_.set(actual_index(value.variable));
                root = value.high;
            }
        }
        current_support_.copy_indices_to(indices);
        return true;
    }

    void add_interval(const support& lower, const support& upper)
    {
        const size_t interval = interval_family(lower, upper);
        covered_ = unite(covered_, interval);
        remaining_ = subtract(remaining_, interval);
    }

    size_t node_count() const noexcept { return nodes_.size(); }

private:
    static constexpr size_t empty = 0;
    static constexpr size_t unit = 1;

    struct node {
        size_t variable;
        size_t low;
        size_t high;
    };

    struct node_key {
        size_t variable;
        size_t low;
        size_t high;

        bool operator==(const node_key& other) const noexcept
        {
            return variable == other.variable && low == other.low && high == other.high;
        }
    };

    struct node_key_hash {
        size_t operator()(const node_key& value) const noexcept
        {
            size_t result = std::hash<size_t>{}(value.variable);
            result ^= std::hash<size_t>{}(value.low) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            result ^= std::hash<size_t>{}(value.high) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            return result;
        }
    };

    struct pair_key {
        size_t left;
        size_t right;

        bool operator==(const pair_key& other) const noexcept
        {
            return left == other.left && right == other.right;
        }
    };

    struct pair_key_hash {
        size_t operator()(const pair_key& value) const noexcept
        {
            size_t result = std::hash<size_t>{}(value.left);
            return result ^ (std::hash<size_t>{}(value.right) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2));
        }
    };

    size_t actual_index(size_t variable) const noexcept
    {
        return dimension_ - 1 - variable;
    }

    size_t variable(size_t root) const noexcept
    {
        return root < 2 ? dimension_ : nodes_[root].variable;
    }

    void operation_checkpoint()
    {
        if ((++operations_ & 4095U) == 0) timeout_checkpoint();
    }

    size_t make_node(size_t variable_value, size_t low, size_t high)
    {
        if (high == empty) return low;
        const node_key key{variable_value, low, high};
        const auto found = unique_.find(key);
        if (found != unique_.end()) return found->second;

        const size_t result = nodes_.size();
        nodes_.push_back({variable_value, low, high});
        unique_.emplace(key, result);
        return result;
    }

    size_t interval_family(const support& lower, const support& upper)
    {
        size_t root = unit;
        for (size_t variable_value = dimension_; variable_value-- > 0;) {
            const size_t bit = actual_index(variable_value);
            if (lower.contains(bit)) {
                root = make_node(variable_value, empty, root);
            } else if (upper.contains(bit)) {
                root = make_node(variable_value, root, root);
            }
        }
        return root;
    }

    size_t cardinality_family(size_t cardinality)
    {
        const size_t missing = std::numeric_limits<size_t>::max();
        std::vector<std::vector<size_t>> memo(dimension_ + 1, std::vector<size_t>(cardinality + 1, missing));
        std::function<size_t(size_t, size_t)> build = [&](size_t variable_value, size_t needed) -> size_t {
            if (needed > dimension_ - variable_value) return empty;
            if (variable_value == dimension_) return needed == 0 ? unit : empty;
            size_t& cached = memo[variable_value][needed];
            if (cached != missing) return cached;

            const size_t low = build(variable_value + 1, needed);
            const size_t high = needed == 0 ? empty : build(variable_value + 1, needed - 1);
            cached = make_node(variable_value, low, high);
            return cached;
        };
        return build(0, cardinality);
    }

    size_t unite(size_t left, size_t right)
    {
        union_cache_.clear();
        return unite_impl(left, right);
    }

    size_t unite_impl(size_t left, size_t right)
    {
        operation_checkpoint();
        if (left == empty || left == right) return right;
        if (right == empty) return left;
        if (right < left) std::swap(left, right);

        const pair_key key{left, right};
        const auto found = union_cache_.find(key);
        if (found != union_cache_.end()) return found->second;

        const size_t left_variable = variable(left);
        const size_t right_variable = variable(right);
        size_t result;
        if (left_variable == right_variable) {
            const node left_node = nodes_[left];
            const node right_node = nodes_[right];
            result = make_node(left_variable,
                               unite_impl(left_node.low, right_node.low),
                               unite_impl(left_node.high, right_node.high));
        } else if (left_variable < right_variable) {
            const node left_node = nodes_[left];
            result = make_node(left_variable, unite_impl(left_node.low, right), left_node.high);
        } else {
            const node right_node = nodes_[right];
            result = make_node(right_variable, unite_impl(left, right_node.low), right_node.high);
        }
        union_cache_.emplace(key, result);
        return result;
    }

    size_t subtract(size_t left, size_t right)
    {
        difference_cache_.clear();
        return subtract_impl(left, right);
    }

    size_t subtract_impl(size_t left, size_t right)
    {
        operation_checkpoint();
        if (left == empty || left == right) return empty;
        if (right == empty) return left;

        const pair_key key{left, right};
        const auto found = difference_cache_.find(key);
        if (found != difference_cache_.end()) return found->second;

        const size_t left_variable = variable(left);
        const size_t right_variable = variable(right);
        size_t result;
        if (left_variable == right_variable) {
            assert(left >= 2 && right >= 2);
            const node left_node = nodes_[left];
            const node right_node = nodes_[right];
            result = make_node(left_variable,
                               subtract_impl(left_node.low, right_node.low),
                               subtract_impl(left_node.high, right_node.high));
        } else if (left_variable < right_variable) {
            assert(left >= 2);
            const node left_node = nodes_[left];
            result = make_node(left_variable, subtract_impl(left_node.low, right), left_node.high);
        } else {
            assert(right >= 2);
            result = subtract_impl(left, nodes_[right].low);
        }
        difference_cache_.emplace(key, result);
        return result;
    }

    size_t dimension_;
    std::vector<node> nodes_;
    std::unordered_map<node_key, size_t, node_key_hash> unique_;
    std::unordered_map<pair_key, size_t, pair_key_hash> union_cache_;
    std::unordered_map<pair_key, size_t, pair_key_hash> difference_cache_;
    size_t covered_ = empty;
    size_t remaining_ = empty;
    support current_support_;
    uint64_t operations_ = 0;
};

class dickinson_checker {
public:
    explicit dickinson_checker(size_t dimension)
        : factorization_(dimension)
        , product_(dimension)
        , supports_(dimension)
        , progress_(progress::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            progress_.stage(subset_dimension);
            supports_.start_cardinality(subset_dimension);
            while (supports_.take_first(indices_)) {
                timeout_checkpoint();
                progress_.visit_support();
                progress_.secondary();
                COPOSIT_INTERVAL_ZDD_TRACE("process", subset_dimension);
                if (!process_subset(matrix)) {
                    progress_.finish();
                    return false;
                }
            }
        }

        progress_.finish();
        return true;
    }

private:
    bool process_subset(const matrix_integer& matrix)
    {
        const size_t dimension = indices_.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices_, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
        if (!singular) {
            for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

            integer denominator;
            factorization_.solve_inplace(solution_, denominator, principal_);
            assert(denominator.sign() > 0);
        } else {
            factorization_.one_nullspace_vector(solution_, principal_);

            bool has_positive_entry = false;
            for (size_t row = 0; row < dimension; ++row) has_positive_entry |= solution_(row, 0).sign() > 0;
            if (!has_positive_entry) solution_.negate();
        }

        bool all_nonpositive = true;
        bool all_nonnegative = singular;
        for (size_t row = 0; row < dimension; ++row) {
            all_nonpositive &= solution_(row, 0).sign() <= 0;
            all_nonnegative &= solution_(row, 0).sign() >= 0;
        }
        if (all_nonpositive || all_nonnegative) return false;

        add_certificate(matrix);
        progress_.certificate();
        return true;
    }

    void add_certificate(const matrix_integer& matrix)
    {
        support lower(matrix.rows());
        support upper(matrix.rows());
        for (size_t local = 0; local < indices_.size(); ++local)
            if (!solution_(local, 0).is_zero()) lower.set(indices_[local]);

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices_.size(); ++local)
                product_[row].addmul(matrix(row, indices_[local]), solution_(local, 0));
            if (product_[row].sign() >= 0) upper.set(row);
        }

        supports_.add_interval(lower, upper);
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column)
                principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    std::vector<integer> product_;
    std::vector<size_t> indices_;
    interval_zdd supports_;
    progress::tracker progress_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    require_strict_mode(mode);
    timeout_checkpoint();
    return dickinson_checker(matrix.rows()).check(matrix);
}

#ifdef COPOSIT_INTERVAL_ZDD_DICKINSON_TESTING
std::pair<size_t, size_t> interval_zdd_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_zdd diagram(dimension);
    for (const auto& [lower_mask, upper_mask] : intervals) {
        support lower(dimension);
        support upper(dimension);
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
        }
        diagram.add_interval(lower, upper);
    }

    diagram.start_cardinality(cardinality);
    std::vector<size_t> indices;
    size_t count = 0;
    while (diagram.take_first(indices)) {
        support exact(dimension);
        for (const size_t index : indices) exact.set(index);
        diagram.add_interval(exact, exact);
        ++count;
    }
    return {count, diagram.node_count()};
}
#endif

} // namespace coposit::model
