#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/progress.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_trace.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

class interval_cbdd {
public:
    explicit interval_cbdd(size_t dimension, progress::tracker* progress = nullptr)
        : dimension_(dimension)
        , progress_(progress)
        , current_support_(dimension)
    {
        nodes_.push_back({dimension_, dimension_, 0, 0}); // Constant false.
        nodes_.push_back({dimension_, dimension_, 1, 1}); // Constant true.
    }

    void start_cardinality(size_t cardinality)
    {
        if (!progress_) {
            remaining_ = subtract(cardinality_family(cardinality), covered_);
            return;
        }
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::cardinality_build);
        remaining_ = subtract(cardinality_family(cardinality), covered_);
        publish_work();
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::support_solve);
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
                current_support_.set(actual_index(value.bottom));
                root = value.high;
            }
        }
        current_support_.copy_indices_to(indices);
        return true;
    }

    void add_interval(const support& lower, const support& upper)
    {
        const size_t certificate = interval_family(lower, upper);
        if (!progress_) {
            covered_ = unite(covered_, certificate);
            remaining_ = subtract(remaining_, certificate);
            return;
        }
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::certificate_union);
        covered_ = unite(covered_, certificate);
        publish_work();
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::certificate_subtract);
        remaining_ = subtract(remaining_, certificate);
        publish_work();
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::support_solve);
    }

    size_t node_count() const noexcept { return nodes_.size(); }

    size_t maximum_chain_length() const noexcept
    {
        size_t result = 0;
        for (size_t root = 2; root < nodes_.size(); ++root)
            result = std::max(result, nodes_[root].bottom - nodes_[root].top + 1);
        return result;
    }

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

        bool operator==(const node_key& other) const noexcept
        {
            return top == other.top && bottom == other.bottom && low == other.low && high == other.high;
        }
    };

    struct node_key_hash {
        size_t operator()(const node_key& value) const noexcept
        {
            size_t result = std::hash<size_t>{}(value.top);
            result ^= std::hash<size_t>{}(value.bottom) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
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

    size_t top(size_t root) const noexcept
    {
        return root < 2 ? dimension_ : nodes_[root].top;
    }

    template <bool ReportProgress>
    void operation_checkpoint()
    {
        if constexpr (ReportProgress) {
            ++operations_;
            if (operations_ % progress::decision_diagram_publish_interval == 0) publish_work();
            if ((operations_ & 4095U) == 0) timeout_checkpoint();
        } else {
            if ((++operations_ & 4095U) == 0) timeout_checkpoint();
        }
    }

    void publish_work() noexcept
    {
        if (progress_) progress_->decision_diagram_work(nodes_.size(), operations_);
    }

    size_t make_node(size_t top_value, size_t bottom_value, size_t low, size_t high)
    {
        if (low == high) return low;

        if (low >= 2) {
            const node child = nodes_[low];
            if (child.top == bottom_value + 1 && child.high == high)
                return make_node(top_value, child.bottom, child.low, high);
        }

        const node_key key{top_value, bottom_value, low, high};
        const auto found = unique_.find(key);
        if (found != unique_.end()) return found->second;

        const size_t result = nodes_.size();
        nodes_.push_back({top_value, bottom_value, low, high});
        unique_.emplace(key, result);
        return result;
    }

    size_t make_node(size_t variable_value, size_t low, size_t high)
    {
        return make_node(variable_value, variable_value, low, high);
    }

    size_t interval_family(const support& lower, const support& upper)
    {
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
        return progress_ ? unite_impl<true>(left, right) : unite_impl<false>(left, right);
    }

    template <bool ReportProgress>
    size_t unite_impl(size_t left, size_t right)
    {
        operation_checkpoint<ReportProgress>();
        if (left == unit || right == unit) return unit;
        if (left == empty || left == right) return right;
        if (right == empty) return left;
        if (right < left) std::swap(left, right);

        const pair_key key{left, right};
        const auto found = union_cache_.find(key);
        if (found != union_cache_.end()) return found->second;

        const size_t top_value = std::min(top(left), top(right));
        const size_t bottom_value = std::min(split_bottom(left, top_value), split_bottom(right, top_value));
        const auto [left_low, left_high] = cofactors(left, bottom_value);
        const auto [right_low, right_high] = cofactors(right, bottom_value);
        const size_t result = make_node(top_value,
                                        bottom_value,
                                        unite_impl<ReportProgress>(left_low, right_low),
                                        unite_impl<ReportProgress>(left_high, right_high));
        union_cache_.emplace(key, result);
        return result;
    }

    size_t subtract(size_t left, size_t right)
    {
        difference_cache_.clear();
        return progress_ ? subtract_impl<true>(left, right) : subtract_impl<false>(left, right);
    }

    template <bool ReportProgress>
    size_t subtract_impl(size_t left, size_t right)
    {
        operation_checkpoint<ReportProgress>();
        if (left == empty || left == right) return empty;
        if (right == empty) return left;
        if (right == unit) return empty;

        const pair_key key{left, right};
        const auto found = difference_cache_.find(key);
        if (found != difference_cache_.end()) return found->second;

        const size_t top_value = std::min(top(left), top(right));
        const size_t bottom_value = std::min(split_bottom(left, top_value), split_bottom(right, top_value));
        const auto [left_low, left_high] = cofactors(left, bottom_value);
        const auto [right_low, right_high] = cofactors(right, bottom_value);
        const size_t result = make_node(top_value,
                                        bottom_value,
                                        subtract_impl<ReportProgress>(left_low, right_low),
                                        subtract_impl<ReportProgress>(left_high, right_high));
        difference_cache_.emplace(key, result);
        return result;
    }

    size_t split_bottom(size_t root, size_t top_value) const noexcept
    {
        if (top(root) == top_value) return nodes_[root].bottom;
        if (root < 2) return dimension_;
        return nodes_[root].top - 1;
    }

    std::pair<size_t, size_t> cofactors(size_t root, size_t bottom_value)
    {
        if (bottom_value < top(root)) return {root, root};

        const node value = nodes_[root];
        if (bottom_value == value.bottom) return {value.low, value.high};
        return {make_node(bottom_value + 1, value.bottom, value.low, value.high), value.high};
    }

    size_t dimension_;
    progress::tracker* progress_;
    std::vector<node> nodes_;
    std::unordered_map<node_key, size_t, node_key_hash> unique_;
    std::unordered_map<pair_key, size_t, pair_key_hash> union_cache_;
    std::unordered_map<pair_key, size_t, pair_key_hash> difference_cache_;
    size_t covered_ = empty;
    size_t remaining_ = empty;
    support current_support_;
    uint64_t operations_ = 0;
};

class maximal_z_blocks {
public:
    explicit maximal_z_blocks(const matrix_integer& matrix)
    {
        adjacency_.reserve(matrix.rows());
        for (size_t index = 0; index < matrix.rows(); ++index) adjacency_.emplace_back(matrix.rows());

        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t column = row + 1; column < matrix.rows(); ++column) {
                if (matrix(row, column).sign() > 0) continue;
                adjacency_[row].set(column);
                adjacency_[column].set(row);
            }
        }
    }

    template <class Visitor>
    bool visit(Visitor&& visitor) const
    {
        const size_t dimension = adjacency_.size();
        support block(dimension);
        support candidates(dimension);
        support excluded(dimension);
        candidates.set_all();
        return search(block, std::move(candidates), std::move(excluded), visitor);
    }

private:
    template <class Visitor>
    bool search(support& block, support candidates, support excluded, Visitor& visitor) const
    {
        timeout_checkpoint();
        if (candidates.empty() && excluded.empty()) {
            std::vector<size_t> indices;
            block.copy_indices_to(indices);
            return indices.size() < 2 || visitor(block, indices);
        }

        const size_t pivot = !candidates.empty() ? candidates.lowest_index() : excluded.lowest_index();
        support extensions = candidates;
        extensions.remove(adjacency_[pivot]);
        std::vector<size_t> vertices;
        extensions.copy_indices_to(vertices);

        for (const size_t vertex : vertices) {
            support child_candidates = candidates;
            support child_excluded = excluded;
            child_candidates.intersect_with(adjacency_[vertex]);
            child_excluded.intersect_with(adjacency_[vertex]);

            block.set(vertex);
            if (!search(block, std::move(child_candidates), std::move(child_excluded), visitor)) return false;
            block.reset(vertex);
            candidates.reset(vertex);
            excluded.set(vertex);
        }
        return true;
    }

    std::vector<support> adjacency_;
};

bool uses_full_certificate_interval(size_t dimension, size_t cardinality, size_t free_indices) noexcept
{
    const size_t remaining = dimension - cardinality;
    const size_t threshold = (remaining / 4) * 3 + ((remaining % 4) * 3) / 4;
    return free_indices > threshold;
}

bool has_motzkin_straus_pattern(const matrix_integer& matrix)
{
    const integer::const_reference nonedge = matrix(0, 0);
    if (nonedge.sign() < 0) return false;

    integer edge;
    bool has_edge = false;
    for (size_t row = 0; row < matrix.rows(); ++row) {
        timeout_checkpoint();
        if (matrix(row, row).compare(nonedge) != 0) return false;
        for (size_t column = row + 1; column < matrix.rows(); ++column) {
            const integer::const_reference value = matrix(row, column);
            if (value.compare(nonedge) == 0) continue;
            if (value.sign() >= 0) return false;
            if (!has_edge) {
                edge = value;
                has_edge = true;
            } else if (value.compare(edge) != 0) {
                return false;
            }
        }
    }
    return has_edge;
}

bool configured_zed_scan()
{
    const char* text = std::getenv("COPOSIT_CBDD_ZED_SCAN");
    if (text == nullptr || std::string_view(text) == "on") return true;
    if (std::string_view(text) == "off") return false;
    throw std::invalid_argument("COPOSIT_CBDD_ZED_SCAN must be 'on' or 'off'");
}

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(mode)
        , progress_(progress::metric::decision_diagram, dimension)
        , supports_(dimension, progress_.active() ? &progress_ : nullptr)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , progress_(progress::metric::decision_diagram, dimension)
        , supports_(dimension, progress_.active() ? &progress_ : nullptr)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            progress_.decision_diagram_cardinality(subset_dimension, progress::decision_diagram_phase::cardinality_build);
            supports_.start_cardinality(subset_dimension);
            while (supports_.take_first(indices_)) {
                timeout_checkpoint();
                progress_.decision_diagram_support();
                COPOSIT_CBDD_ZED_TRACE("process", subset_dimension);
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
    bool process_z_block(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        std::vector<bool> reached(indices.size());
        std::vector<size_t> queue;
        std::vector<size_t> component;
        for (size_t start = 0; start < indices.size(); ++start) {
            if (reached[start]) continue;
            queue.assign(1, start);
            reached[start] = true;
            component.clear();
            for (size_t next = 0; next < queue.size(); ++next) {
                const size_t local = queue[next];
                component.push_back(indices[local]);
                for (size_t candidate = 0; candidate < indices.size(); ++candidate) {
                    if (!reached[candidate] && matrix(indices[local], indices[candidate]).sign() < 0) {
                        reached[candidate] = true;
                        queue.push_back(candidate);
                    }
                }
            }

            principal_.resize(component.size(), component.size());
            copy_principal(matrix, component, principal_);
            factorization_.factorize_inplace(principal_);
            COPOSIT_CBDD_ZED_TRACE("z-component", component.size());
            const bool positive_semidefinite = factorization_.is_positive_semidefinite();
            const bool positive_definite = positive_semidefinite && factorization_.is_positive_definite();
            if (classification_ != nullptr) {
                if (!positive_semidefinite) {
                    COPOSIT_CBDD_ZED_TRACE("z-reject", indices.size());
                    return false;
                }
                if (!positive_definite) classification_->is_strictly_copositive = false;
            } else if (!(mode_ == copositivity_mode::strictly_copositive ? positive_definite : positive_semidefinite)) {
                COPOSIT_CBDD_ZED_TRACE("z-reject", indices.size());
                return false;
            }
        }

        COPOSIT_CBDD_ZED_TRACE("z-pass", indices.size());
        return true;
    }

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
        if (all_nonpositive) return false;
        if (all_nonnegative) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

        const size_t free_indices = add_certificate(matrix);
        if (progress_.active()) progress_.decision_diagram_certificate(free_indices);
        return true;
    }

    size_t add_certificate(const matrix_integer& matrix)
    {
        support lower(matrix.rows());
        support upper(matrix.rows());
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (!solution_(local, 0).is_zero()) {
                lower.set(indices_[local]);
                ++lower_size;
            }
        }

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices_.size(); ++local)
                product_[row].addmul(matrix(row, indices_[local]), solution_(local, 0));
            if (product_[row].sign() >= 0) {
                upper.set(row);
                ++upper_size;
            }
        }

        const size_t free_indices = upper_size - lower_size;
        if (uses_full_certificate_interval(matrix.rows(), indices_.size(), free_indices)) {
            supports_.add_interval(lower, upper);
            COPOSIT_CBDD_ZED_TRACE("wide-certificate", free_indices);
        } else {
            support processed(matrix.rows());
            for (const size_t index : indices_) processed.set(index);
            supports_.add_interval(processed, processed);
            COPOSIT_CBDD_ZED_TRACE("narrow-certificate", free_indices);
        }
        return free_indices;
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
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    progress::tracker progress_;
    interval_cbdd supports_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    return dickinson_checker(matrix.rows(), mode).check(matrix);
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    copositivity_classification result{true, true};
    if (!dickinson_checker(matrix.rows(), result).check(matrix)) result = {false, false};
    return result;
}

#ifdef COPOSIT_CBDD_ZED_DICKINSON_TESTING
bool wide_certificate_cbdd_uses_full_interval(size_t dimension, size_t cardinality, size_t free_indices) noexcept
{
    return uses_full_certificate_interval(dimension, cardinality, free_indices);
}

std::pair<size_t, size_t> cbdd_zed_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_cbdd diagram(dimension);
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

size_t cbdd_zed_maximum_interval_chain(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    interval_cbdd diagram(dimension);
    support lower(dimension);
    support upper(dimension);
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.maximum_chain_length();
}
#endif

} // namespace coposit::model
