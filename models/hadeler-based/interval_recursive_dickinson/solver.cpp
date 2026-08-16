#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

struct certificate_interval {
    certificate_interval(const support& lower_value, const support& upper_value)
        : lower(lower_value), upper(upper_value)
    {}

    support lower;
    support upper;
};

class interval_generator {
public:
    explicit interval_generator(size_t dimension)
        : dimension_(dimension)
        , by_trigger_(dimension)
        , by_lowest_lower_(dimension)
        , partial_(dimension)
    {}

    void add_interval(const support& lower, const support& upper)
    {
        const size_t interval_index = intervals_.size();
        intervals_.emplace_back(lower, upper);

        size_t trigger = dimension_;
        for (size_t bit = 0; bit < dimension_; ++bit) {
            if (lower.contains(bit) || !upper.contains(bit)) {
                trigger = bit;
                break;
            }
        }
        assert(trigger < dimension_);
        by_trigger_[trigger].push_back(interval_index);
        by_lowest_lower_[lower.lowest_index()].push_back(interval_index);
    }

    template<class Callback>
    bool generate(size_t cardinality, Callback&& callback)
    {
        return generate_from(dimension_, cardinality, callback);
    }

private:
    bool covered_complete_support() const noexcept
    {
        for (size_t bit = 0; bit < dimension_; ++bit) {
            if (!partial_.contains(bit)) continue;
            for (const size_t interval_index : by_lowest_lower_[bit]) {
                const certificate_interval& interval = intervals_[interval_index];
                if (interval.lower.is_subset_of(partial_) && partial_.is_subset_of(interval.upper)) return true;
            }
        }
        return false;
    }

    bool completes_interval(size_t trigger) const noexcept
    {
        for (const size_t interval_index : by_trigger_[trigger]) {
            const certificate_interval& interval = intervals_[interval_index];
            if (interval.lower.is_subset_of(partial_) && partial_.is_subset_of(interval.upper)) return true;
        }
        return false;
    }

    bool interval_covers_branch(const certificate_interval& interval, size_t bits_remaining, size_t needed) const noexcept
    {
        if (!partial_.is_subset_of(interval.upper)) return false;

        bool free_required = false;
        bool free_forbidden = false;
        for (size_t bit = 0; bit < bits_remaining; ++bit) {
            free_required |= interval.lower.contains(bit);
            free_forbidden |= !interval.upper.contains(bit);
        }
        for (size_t bit = bits_remaining; bit < dimension_; ++bit) {
            if (interval.lower.contains(bit) && !partial_.contains(bit)) return false;
        }

        if (free_required && needed != bits_remaining) return false;
        if (free_forbidden && needed != 0) return false;
        return true;
    }

    bool new_interval_covers_branch(size_t first_new, size_t bits_remaining, size_t needed) const noexcept
    {
        for (size_t interval_index = first_new; interval_index < intervals_.size(); ++interval_index) {
            if (interval_covers_branch(intervals_[interval_index], bits_remaining, needed)) return true;
        }
        return false;
    }

    template<class Callback>
    bool generate_from(size_t bits_remaining, size_t needed, Callback& callback)
    {
        timeout_checkpoint();
        if (needed > bits_remaining) return true;
        if (needed == 0) {
            if (covered_complete_support()) return true;
            return callback(partial_);
        }

        const size_t bit = bits_remaining - 1;
        const size_t interval_count_before_exclusion = intervals_.size();
        if (needed < bits_remaining && !completes_interval(bit) && !generate_from(bit, needed, callback)) return false;

        partial_.set(bit);
        const bool covered = completes_interval(bit)
            || new_interval_covers_branch(interval_count_before_exclusion, bit, needed - 1);
        const bool keep_going = covered || generate_from(bit, needed - 1, callback);
        partial_.reset(bit);
        return keep_going;
    }

    size_t dimension_;
    std::vector<certificate_interval> intervals_;
    std::vector<std::vector<size_t>> by_trigger_;
    std::vector<std::vector<size_t>> by_lowest_lower_;
    support partial_;
};

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : factorization_(dimension)
        , product_(dimension)
        , generator_(dimension)
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , product_(dimension)
        , generator_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            diagnostics_.stage(subset_dimension);
            const bool keep_going = generator_.generate(subset_dimension, [&](const support& current_support) {
                diagnostics_.visit_support();
                diagnostics_.secondary();
                COPOSIT_INTERVAL_RECURSIVE_DIAGNOSTICS("process", subset_dimension);
                current_support.copy_indices_to(indices_);
                if (!process_subset(matrix)) {
                    diagnostics_.finish();
                    return false;
                }
                return true;
            });
            if (!keep_going) return false;
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

        add_certificate(matrix);
        diagnostics_.certificate();
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

        generator_.add_interval(lower, upper);
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
    interval_generator generator_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
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

#ifdef COPOSIT_INTERVAL_RECURSIVE_DICKINSON_TESTING
size_t interval_recursive_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_generator generator(dimension);
    for (const auto& [lower_mask, upper_mask] : intervals) {
        support lower(dimension);
        support upper(dimension);
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
        }
        generator.add_interval(lower, upper);
    }

    size_t count = 0;
    generator.generate(cardinality, [&](const support&) {
        ++count;
        return true;
    });
    return count;
}
#endif

} // namespace coposit::model
