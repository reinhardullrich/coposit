#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
size_t level_two_skips = 0;
#endif

class negative_graph {
public:
    explicit negative_graph(const matrix_integer& matrix)
        : support_context_(matrix.rows())
        , unreached_(support_context_.make())
        , frontier_(support_context_.make())
        , next_(support_context_.make())
    {
        neighbors_.reserve(matrix.rows());
        for (size_t vertex = 0; vertex < matrix.rows(); ++vertex) neighbors_.push_back(support_context_.make());
        frontier_indices_.reserve(matrix.rows());

        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t column = row + 1; column < matrix.rows(); ++column) {
                if (matrix(row, column).sign() < 0) {
                    support_context_.set(neighbors_[row], column);
                    support_context_.set(neighbors_[column], row);
                } else {
                    complete_ = false;
                }
            }
        }
    }

    bool induced_is_connected(const support& vertices)
    {
        if (complete_) return true;
        support_context_.copy(unreached_, vertices);
        const size_t root = support_context_.first(unreached_);
        support_context_.reset(unreached_, root);
        if (support_context_.empty(unreached_)) return true;
        if (support_context_.is_subset_of(unreached_, neighbors_[root])) return true;

        support_context_.clear(frontier_);
        support_context_.set(frontier_, root);
        while (!support_context_.empty(unreached_)) {
            timeout_checkpoint();
            support_context_.clear(next_);
            support_context_.extract_set_indices(frontier_, frontier_indices_);
            for (const size_t vertex : frontier_indices_) support_context_.add(next_, neighbors_[vertex]);
            support_context_.intersect(next_, unreached_);
            if (support_context_.empty(next_)) return false;
            support_context_.subtract(unreached_, next_);
            support_context_.swap(frontier_, next_);
        }
        return true;
    }

private:
    support_context support_context_;
    std::vector<support> neighbors_;
    support unreached_;
    support frontier_;
    support next_;
    std::vector<size_t> frontier_indices_;
    bool complete_ = true;
};

class support_generator {
public:
    explicit support_generator(size_t dimension)
        : support_context_(dimension)
        , dimension_(dimension)
        , forbidden_by_lowest_(dimension)
        , partial_support_(support_context_.make())
    {
    }

    template<class Callback>
    void generate(Callback&& callback)
    {
        for (target_cardinality_ = 1; target_cardinality_ <= dimension_; ++target_cardinality_) {
            activate_pending();
            emitted_ = false;
            if (!generate_from(dimension_, target_cardinality_, callback)) return;
            if (!emitted_) return;
        }
    }

    void add_forbidden(const support& current_support)
    {
        pending_forbidden_.push_back(support_context_.clone(current_support));
    }

private:
    void activate_pending()
    {
        for (support& forbidden : pending_forbidden_)
            forbidden_by_lowest_[support_context_.first(forbidden)].push_back(std::move(forbidden));
        pending_forbidden_.clear();
    }

    bool completes_forbidden(size_t new_lowest_bit) const noexcept
    {
        for (const support& forbidden : forbidden_by_lowest_[new_lowest_bit]) {
            if (support_context_.is_subset_of(forbidden, partial_support_)) return true;
        }
        return false;
    }

    template<class Callback>
    bool generate_from(size_t bits_remaining, size_t needed, Callback& callback)
    {
        timeout_checkpoint();
        if (needed == 0) {
            emitted_ = true;
            return callback(partial_support_, target_cardinality_);
        }
        if (needed > bits_remaining) return true;

        const size_t bit = bits_remaining - 1;
        if (needed < bits_remaining && !generate_from(bit, needed, callback)) return false;

        support_context_.set(partial_support_, bit);
        const bool keep_going = completes_forbidden(bit) || generate_from(bit, needed - 1, callback);
        support_context_.reset(partial_support_, bit);
        return keep_going;
    }

    support_context support_context_;
    size_t dimension_;
    std::vector<std::vector<support>> forbidden_by_lowest_;
    std::vector<support> pending_forbidden_;
    support partial_support_;
    size_t target_cardinality_ = 0;
    bool emitted_ = false;
};

class candidate_search {
public:
    explicit candidate_search(const matrix_integer& matrix)
        : support_context_(matrix.rows())
        , input_(matrix)
        , game_(matrix)
        , factorization_(matrix.rows())
        , negative_graph_(matrix)
    {
        game_.negate();
        support_indices_.reserve(matrix.rows());
    }

    copositivity_classification classify(bool stop_at_zero)
    {
        support_generator generator(game_.rows());
        copositivity_classification result{true, true};
        generator.generate([&](const support& current_support, size_t support_size) {
            support_context_.extract_set_indices(current_support, support_indices_);
            if (support_size <= 3) {
                const copositivity_classification face =
                    small_copositivity::classify_principal(input_, support_indices_.data(), support_size);
                if (!face.is_copositive) {
                    result = {false, false};
                    return false;
                }
                if (!face.is_strictly_copositive) {
                    result.is_strictly_copositive = false;
                    if (stop_at_zero) return false;
                }
            }
            if (support_size > 3 && !negative_graph_.induced_is_connected(current_support)) {
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
                ++level_two_skips;
#endif
                return true;
            }
            if (!find_candidate(current_support, support_size)) return true;
            if (payoff_numerator_.sign() > 0) {
                result = {false, false};
                return false;
            }
            if (payoff_numerator_.sign() == 0) {
                result.is_strictly_copositive = false;
                if (stop_at_zero) return false;
            }
            generator.add_forbidden(current_support);
            return true;
        });
        return result;
    }

private:
    void build_reduced_system(size_t support_size)
    {
        const size_t reduced_dimension = support_size - 1;
        reduced_system_.resize(reduced_dimension, reduced_dimension);
        solution_numerators_.resize(reduced_dimension, 1);

        const size_t reference = support_indices_[0];
        const auto reference_diagonal = game_(reference, reference);
        for (size_t row = 0; row < reduced_dimension; ++row) {
            timeout_checkpoint();
            const size_t i = support_indices_[row + 1];
            solution_numerators_(row, 0).set_difference(reference_diagonal, game_(i, reference));
            for (size_t column = 0; column <= row; ++column) {
                const size_t j = support_indices_[column + 1];
                reduced_system_(row, column).set_difference(game_(i, j), game_(reference, j));
                reduced_system_(row, column) += solution_numerators_(row, 0);
            }
        }
    }

    void calculate_payoff(integer& payoff, size_t strategy, size_t support_size)
    {
        const size_t reference = support_indices_[0];
        payoff.set_product(game_(strategy, reference), reference_numerator_);
        for (size_t position = 1; position < support_size; ++position) {
            payoff.addmul(game_(strategy, support_indices_[position]), solution_numerators_(position - 1, 0));
        }
    }

    bool find_candidate(const support& current_support, size_t support_size)
    {
        timeout_checkpoint();
        assert(support_indices_.size() == support_size);

        if (support_size == 1) {
            solution_denominator_.set_one();
            reference_numerator_.set_one();
        } else {
            build_reduced_system(support_size);
            if (!factorization_.factorize_inplace(reduced_system_)) return false;
            factorization_.solve_inplace(solution_numerators_, solution_denominator_, reduced_system_);
            assert(solution_denominator_.sign() > 0);

            reference_numerator_ = solution_denominator_;
            for (size_t position = 1; position < support_size; ++position) {
                const auto numerator = solution_numerators_(position - 1, 0);
                if (numerator.sign() <= 0) return false;
                reference_numerator_ -= numerator;
            }
            if (reference_numerator_.sign() <= 0) return false;
        }

        const size_t reference = support_indices_[0];
        calculate_payoff(payoff_numerator_, reference, support_size);
        for (size_t strategy = 0; strategy < game_.rows(); ++strategy) {
            if (support_context_.contains(current_support, strategy)) continue;
            calculate_payoff(outside_payoff_numerator_, strategy, support_size);
            if (outside_payoff_numerator_.compare(payoff_numerator_) > 0) return false;
        }

        return true;
    }

    support_context support_context_;
    const matrix_integer& input_;
    matrix_integer game_;
    fraction_free_ldlt_factorization factorization_;
    negative_graph negative_graph_;
    std::vector<size_t> support_indices_;
    matrix_integer reduced_system_;
    matrix_integer solution_numerators_;
    integer solution_denominator_;
    integer reference_numerator_;
    integer payoff_numerator_;
    integer outside_payoff_numerator_;
};

} // namespace

#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
size_t level_two_skips_for_testing() noexcept
{
    return level_two_skips;
}
#endif

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension <= 3) return small_copositivity::check(matrix, mode);
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
    level_two_skips = 0;
#endif
    const copositivity_classification result = candidate_search(matrix).classify(mode == copositivity_mode::strictly_copositive);
    return mode == copositivity_mode::copositive ? result.is_copositive : result.is_strictly_copositive;
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    if (matrix.rows() <= 3) return small_copositivity::classify(matrix);
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
    level_two_skips = 0;
#endif
    return candidate_search(matrix).classify(false);
}

} // namespace coposit::model
