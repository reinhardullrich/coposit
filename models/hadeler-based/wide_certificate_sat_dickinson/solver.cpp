#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cadical.hpp>

#include "source_diagnostics.hpp"

#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

thread_local std::optional<size_t> certificate_threshold_percentage;

bool uses_full_certificate_interval(size_t dimension, size_t cardinality, size_t free_indices)
{
    if (!certificate_threshold_percentage) throw std::logic_error("wide-certificate percentage is not configured");
    const size_t remaining = dimension - cardinality;
    const size_t threshold = (remaining / 100) * *certificate_threshold_percentage
        + ((remaining % 100) * *certificate_threshold_percentage) / 100;
    return free_indices > threshold;
}

class timeout_terminator final : public CaDiCaL::Terminator {
public:
    bool terminate() override { return timeout_pending(); }
};

class interval_sat {
public:
    explicit interval_sat(const support_context& context)
        : context_(context)
        , dimension_(context.dimension())
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
        size_t upper_size = 0;
#ifdef COPOSIT_WIDE_CERTIFICATE_SAT_DICKINSON_TESTING
        last_interval_clause_size_ = 0;
#endif
        for (size_t index = 0; index < dimension_; ++index) {
            const bool in_upper = context_.contains(upper, index);
            if (in_upper) ++upper_size;
            if (context_.contains(lower, index)) {
                solver_.add(-variable(index));
#ifdef COPOSIT_WIDE_CERTIFICATE_SAT_DICKINSON_TESTING
                ++last_interval_clause_size_;
#endif
            } else if (!in_upper) {
                solver_.add(variable(index));
#ifdef COPOSIT_WIDE_CERTIFICATE_SAT_DICKINSON_TESTING
                ++last_interval_clause_size_;
#endif
            }
        }
        if (upper_size < dimension_) {
            solver_.add(cardinality_outputs_[upper_size]);
#ifdef COPOSIT_WIDE_CERTIFICATE_SAT_DICKINSON_TESTING
            ++last_interval_clause_size_;
#endif
        }
        solver_.add(0);
        ++interval_count_;
    }

    size_t interval_count() const noexcept { return interval_count_; }
#ifdef COPOSIT_WIDE_CERTIFICATE_SAT_DICKINSON_TESTING
    size_t last_interval_clause_size() const noexcept { return last_interval_clause_size_; }
#endif

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

    const support_context& context_;


    size_t dimension_;
    int next_variable_ = 1;
    size_t cardinality_ = 0;
    size_t interval_count_ = 0;
#ifdef COPOSIT_WIDE_CERTIFICATE_SAT_DICKINSON_TESTING
    size_t last_interval_clause_size_ = 0;
#endif
    timeout_terminator terminator_;
    CaDiCaL::Solver solver_;
    std::vector<int> cardinality_outputs_;
};

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        supports_.emplace(support_context_);
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            diagnostics_.stage(subset_dimension);
            supports_->start_cardinality(subset_dimension);
            while (supports_->take_first(indices_)) {
                timeout_checkpoint();
                diagnostics_.visit_support();
                diagnostics_.secondary();
                COPOSIT_WIDE_CERTIFICATE_SAT_DIAGNOSTICS("process", subset_dimension);
                if (!process_subset(matrix)) {
                    diagnostics_.finish();
                    return false;
                }
            }
        }

        diagnostics_.finish();
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
        if (all_nonpositive) return false;
        if (all_nonnegative) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

        const auto [free_indices, upper_size] = add_certificate(matrix);
        if (diagnostics_.active()) diagnostics_.certificate(free_indices, upper_size);
        return true;
    }

    std::pair<size_t, size_t> add_certificate(const matrix_integer& matrix)
    {
        support lower = support_context_.make();
        support upper = support_context_.make();
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (!solution_(local, 0).is_zero()) {
                support_context_.set(lower, indices_[local]);
                ++lower_size;
            }
        }

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices_.size(); ++local)
                product_[row].addmul(matrix(row, indices_[local]), solution_(local, 0));
            if (product_[row].sign() >= 0) {
                support_context_.set(upper, row);
                ++upper_size;
            }
        }

        const size_t free_indices = upper_size - lower_size;
        if (uses_full_certificate_interval(matrix.rows(), indices_.size(), free_indices)) {
            supports_->add_interval(lower, upper);
            COPOSIT_WIDE_CERTIFICATE_SAT_DIAGNOSTICS("wide-certificate", free_indices);
        } else {
            support_context_.clear(lower);
            support_context_.clear(upper);
            for (const size_t index : indices_) {
                support_context_.set(lower, index);
                support_context_.set(upper, index);
            }
            supports_->add_interval(lower, upper);
            COPOSIT_WIDE_CERTIFICATE_SAT_DIAGNOSTICS("narrow-certificate", free_indices);
        }
        support_context_.release(std::move(lower));
        support_context_.release(std::move(upper));
        return {free_indices, upper_size};
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column)
                principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

    support_context support_context_;

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    std::vector<integer> product_;
    std::vector<size_t> indices_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    std::optional<interval_sat> supports_;
};

} // namespace

void configure(std::string_view parameter)
{
    size_t percentage = 0;
    const auto [end, error] = std::from_chars(parameter.data(), parameter.data() + parameter.size(), percentage);
    if (error != std::errc{} || end != parameter.data() + parameter.size() || percentage > 100)
        throw std::invalid_argument("wide-certificate percentage must be an integer from 0 through 100");
    certificate_threshold_percentage = percentage;
}

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

#ifdef COPOSIT_WIDE_CERTIFICATE_SAT_DICKINSON_TESTING
bool wide_certificate_sat_uses_full_interval(size_t dimension, size_t cardinality, size_t free_indices)
{
    return uses_full_certificate_interval(dimension, cardinality, free_indices);
}

size_t wide_certificate_sat_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    support_context support_context_(dimension);
    interval_sat sat(support_context_);
    for (const auto& [lower_mask, upper_mask] : intervals) {
        support lower = support_context_.make();
        support upper = support_context_.make();
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) support_context_.set(lower, bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) support_context_.set(upper, bit);
        }
        sat.add_interval(lower, upper);
    }

    sat.start_cardinality(cardinality);
    std::vector<size_t> indices;
    size_t count = 0;
    while (sat.take_first(indices)) {
        support exact = support_context_.make();
        for (const size_t index : indices) support_context_.set(exact, index);
        sat.add_interval(exact, exact);
        ++count;
    }
    return count;
}

size_t wide_certificate_sat_interval_count(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    support_context support_context_(dimension);
    interval_sat sat(support_context_);
    support lower = support_context_.make();
    support upper = support_context_.make();
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) support_context_.set(lower, bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) support_context_.set(upper, bit);
    }
    sat.add_interval(lower, upper);
    return sat.interval_count();
}

size_t wide_certificate_sat_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    support_context support_context_(dimension);
    interval_sat sat(support_context_);
    support lower = support_context_.make();
    support upper = support_context_.make();
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) support_context_.set(lower, bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) support_context_.set(upper, bit);
    }
    sat.add_interval(lower, upper);
    return sat.last_interval_clause_size();
}
#endif

} // namespace coposit::model
