#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/progress.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cadical.hpp>

#include "source_trace.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

class timeout_terminator final : public CaDiCaL::Terminator {
public:
    bool terminate() override { return timeout_pending(); }
};

class interval_sat {
public:
    explicit interval_sat(size_t dimension)
        : dimension_(dimension)
    {
        if (dimension_ > static_cast<size_t>(std::numeric_limits<int>::max() - 1))
            throw std::overflow_error("SAT variable count exceeds CaDiCaL's integer literal range");
        next_variable_ = static_cast<int>(dimension_) + 1;

        if (!solver_.configure("sat")) throw std::runtime_error("CaDiCaL lacks its satisfiable-instance configuration");
        if (!solver_.set("ilb", 2)) throw std::runtime_error("CaDiCaL lacks incremental lazy backtracking");
        solver_.connect_terminator(&terminator_);
        std::vector<int> wires;
        size_t padded_dimension = 1;
        while (padded_dimension < dimension_) {
            if (padded_dimension > static_cast<size_t>(std::numeric_limits<int>::max()) / 2)
                throw std::overflow_error("SAT cardinality network is too large");
            padded_dimension *= 2;
        }
        wires.reserve(padded_dimension);
        for (size_t index = 0; index < dimension_; ++index) wires.push_back(variable(index));

        if (padded_dimension != dimension_) {
            const int constant_false = new_variable();
            add_clause({-constant_false});
            wires.resize(padded_dimension, constant_false);
        }

        bitonic_sort(wires, 0, wires.size(), true);
        cardinality_outputs_.assign(wires.begin(), wires.begin() + dimension_);
    }

    void start_cardinality(size_t cardinality) noexcept { cardinality_ = cardinality; }

    bool take_first(std::vector<size_t>& indices)
    {
        assert(cardinality_ >= 1 && cardinality_ <= dimension_);
        solver_.assume(cardinality_outputs_[cardinality_ - 1]);
        if (cardinality_ < dimension_) solver_.assume(-cardinality_outputs_[cardinality_]);

        const int status = solver_.solve();
        if (status == CaDiCaL::UNSATISFIABLE) return false;
        if (status != CaDiCaL::SATISFIABLE) {
            timeout_checkpoint();
            throw std::runtime_error("CaDiCaL returned an inconclusive result without a coposit timeout");
        }

        indices.clear();
        for (size_t index = 0; index < dimension_; ++index)
            if (solver_.val(variable(index)) > 0) indices.push_back(index);
        assert(indices.size() == cardinality_);
        return true;
    }

    void add_interval(const support& lower, const support& upper)
    {
        for (size_t index = 0; index < dimension_; ++index) {
            if (lower.contains(index)) solver_.add(-variable(index));
            else if (!upper.contains(index)) solver_.add(variable(index));
        }
        solver_.add(0);
        ++interval_count_;
    }

    size_t interval_count() const noexcept { return interval_count_; }

private:
    int variable(size_t index) const noexcept
    {
        return static_cast<int>(index) + 1;
    }

    int new_variable()
    {
        if (next_variable_ == std::numeric_limits<int>::max())
            throw std::overflow_error("SAT cardinality network exceeds CaDiCaL's integer literal range");
        if ((next_variable_ & 4095) == 0) timeout_checkpoint();
        return next_variable_++;
    }

    void add_clause(std::initializer_list<int> literals)
    {
        for (const int literal : literals) solver_.add(literal);
        solver_.add(0);
    }

    std::pair<int, int> comparator(int first, int second)
    {
        const int high = new_variable();
        const int low = new_variable();
        add_clause({-first, high});
        add_clause({-second, high});
        add_clause({first, second, -high});
        add_clause({first, -low});
        add_clause({second, -low});
        add_clause({-first, -second, low});
        return {high, low};
    }

    void compare_exchange(std::vector<int>& wires, size_t first, size_t second, bool descending)
    {
        const auto [high, low] = comparator(wires[first], wires[second]);
        wires[first] = descending ? high : low;
        wires[second] = descending ? low : high;
    }

    void bitonic_merge(std::vector<int>& wires, size_t first, size_t count, bool descending)
    {
        if (count < 2) return;
        const size_t half = count / 2;
        for (size_t index = first; index < first + half; ++index)
            compare_exchange(wires, index, index + half, descending);
        bitonic_merge(wires, first, half, descending);
        bitonic_merge(wires, first + half, half, descending);
    }

    void bitonic_sort(std::vector<int>& wires, size_t first, size_t count, bool descending)
    {
        if (count < 2) return;
        const size_t half = count / 2;
        bitonic_sort(wires, first, half, !descending);
        bitonic_sort(wires, first + half, half, descending);
        bitonic_merge(wires, first, count, descending);
    }

    size_t dimension_;
    int next_variable_ = 1;
    size_t cardinality_ = 0;
    size_t interval_count_ = 0;
    timeout_terminator terminator_;
    CaDiCaL::Solver solver_;
    std::vector<int> cardinality_outputs_;
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
    const char* text = std::getenv("COPOSIT_SAT_ZED_SCAN");
    if (text == nullptr || std::string_view(text) == "on") return true;
    if (std::string_view(text) == "off") return false;
    throw std::invalid_argument("COPOSIT_SAT_ZED_SCAN must be 'on' or 'off'");
}

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(mode)
        , progress_(progress::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , progress_(progress::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        supports_.emplace(matrix.rows());
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            progress_.stage(subset_dimension);
            supports_->start_cardinality(subset_dimension);
            while (supports_->take_first(indices_)) {
                timeout_checkpoint();
                progress_.visit_support();
                progress_.secondary();
                COPOSIT_SAT_ZED_TRACE("process", subset_dimension);
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
            COPOSIT_SAT_ZED_TRACE("z-component", component.size());
            const bool positive_semidefinite = factorization_.is_positive_semidefinite();
            const bool positive_definite = positive_semidefinite && factorization_.is_positive_definite();
            if (classification_ != nullptr) {
                if (!positive_semidefinite) {
                    COPOSIT_SAT_ZED_TRACE("z-reject", indices.size());
                    return false;
                }
                if (!positive_definite) classification_->is_strictly_copositive = false;
            } else if (!(mode_ == copositivity_mode::strictly_copositive ? positive_definite : positive_semidefinite)) {
                COPOSIT_SAT_ZED_TRACE("z-reject", indices.size());
                return false;
            }
        }

        COPOSIT_SAT_ZED_TRACE("z-pass", indices.size());
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

        if (progress_.active()) {
            const auto [free_indices, upper_size] = add_certificate<true>(matrix);
            progress_.certificate(free_indices, upper_size);
        } else {
            add_certificate<false>(matrix);
        }
        return true;
    }

    template <bool CountSizes>
    std::pair<size_t, size_t> add_certificate(const matrix_integer& matrix)
    {
        support lower(matrix.rows());
        support upper(matrix.rows());
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (!solution_(local, 0).is_zero()) {
                lower.set(indices_[local]);
                if constexpr (CountSizes) ++lower_size;
            }
        }

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices_.size(); ++local)
                product_[row].addmul(matrix(row, indices_[local]), solution_(local, 0));
            if (product_[row].sign() >= 0) {
                upper.set(row);
                if constexpr (CountSizes) ++upper_size;
            }
        }

        supports_->add_interval(lower, upper);
        if constexpr (CountSizes) return {upper_size - lower_size, upper_size};
        return {0, 0};
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
    std::optional<interval_sat> supports_;
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

#ifdef COPOSIT_SAT_ZED_DICKINSON_TESTING
size_t sat_zed_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_sat diagram(dimension);
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
    return count;
}

size_t sat_zed_interval_count(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    interval_sat diagram(dimension);
    support lower(dimension);
    support upper(dimension);
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.interval_count();
}
#endif

} // namespace coposit::model
