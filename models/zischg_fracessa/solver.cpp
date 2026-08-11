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
        : unreached_(matrix.rows())
        , frontier_(matrix.rows())
        , next_(matrix.rows())
    {
        neighbors_.reserve(matrix.rows());
        for (size_t vertex = 0; vertex < matrix.rows(); ++vertex) neighbors_.emplace_back(matrix.rows());
        frontier_indices_.reserve(matrix.rows());

        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t column = row + 1; column < matrix.rows(); ++column) {
                if (matrix(row, column).sign() < 0) {
                    neighbors_[row].set(column);
                    neighbors_[column].set(row);
                } else {
                    complete_ = false;
                }
            }
        }
    }

    bool induced_is_connected(const support& vertices)
    {
        if (complete_) return true;
        unreached_ = vertices;
        const size_t root = unreached_.lowest_index();
        unreached_.reset(root);
        if (unreached_.empty()) return true;
        if (unreached_.is_subset_of(neighbors_[root])) return true;

        frontier_.clear();
        frontier_.set(root);
        while (!unreached_.empty()) {
            timeout_checkpoint();
            next_.clear();
            frontier_.copy_indices_to(frontier_indices_);
            for (const size_t vertex : frontier_indices_) next_.add(neighbors_[vertex]);
            next_.intersect_with(unreached_);
            if (next_.empty()) return false;
            unreached_.remove(next_);
            frontier_.swap(next_);
        }
        return true;
    }

private:
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
        : dimension_(dimension)
        , forbidden_by_lowest_(dimension)
        , partial_support_(dimension)
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
        pending_forbidden_.push_back(current_support);
    }

private:
    void activate_pending()
    {
        for (support& forbidden : pending_forbidden_) forbidden_by_lowest_[forbidden.lowest_index()].push_back(std::move(forbidden));
        pending_forbidden_.clear();
    }

    bool completes_forbidden(size_t new_lowest_bit) const noexcept
    {
        for (const support& forbidden : forbidden_by_lowest_[new_lowest_bit]) {
            if (forbidden.is_subset_of(partial_support_)) return true;
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

        partial_support_.set(bit);
        const bool keep_going = completes_forbidden(bit) || generate_from(bit, needed - 1, callback);
        partial_support_.reset(bit);
        return keep_going;
    }

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
        : input_(matrix)
        , game_(matrix)
        , factorization_(matrix.rows())
        , negative_graph_(matrix)
    {
        game_.negate();
        support_indices_.reserve(matrix.rows());
    }

    bool solve()
    {
        support_generator generator(game_.rows());
        bool strictly_copositive = true;
        generator.generate([&](const support& current_support, size_t support_size) {
            current_support.copy_indices_to(support_indices_);
            if (support_size <= 3
                && !small_copositivity::check_principal(input_, support_indices_.data(), support_size)) {
                strictly_copositive = false;
                return false;
            }
            if (support_size > 3 && !negative_graph_.induced_is_connected(current_support)) {
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
                ++level_two_skips;
#endif
                return true;
            }
            if (!find_candidate(current_support, support_size)) return true;
            if (payoff_numerator_.sign() >= 0) {
                strictly_copositive = false;
                return false;
            }
            generator.add_forbidden(current_support);
            return true;
        });
        if (!strictly_copositive) return false;
        return true;
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
            if (current_support.contains(strategy)) continue;
            calculate_payoff(outside_payoff_numerator_, strategy, support_size);
            if (outside_payoff_numerator_.compare(payoff_numerator_) > 0) return false;
        }

        return true;
    }

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
    require_strict_mode(mode);
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension == 0 || matrix.cols() != dimension) throw std::invalid_argument("matrix must be nonempty and square");
    for (size_t row = 0; row < dimension; ++row) {
        for (size_t column = row + 1; column < dimension; ++column) {
            if (matrix(row, column).compare(matrix(column, row)) != 0) throw std::invalid_argument("matrix must be symmetric");
        }
    }

    if (dimension <= 3) return small_copositivity::check(matrix);
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
    level_two_skips = 0;
#endif
    return candidate_search(matrix).solve();
}

} // namespace coposit::model
