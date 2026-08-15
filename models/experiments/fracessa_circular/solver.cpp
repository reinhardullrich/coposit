#include "circular_affine_symmetry.hpp"
#include "circular_support_generator.hpp"

#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/progress.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace coposit::model {

namespace {

/*
 * First-order-only adaptation of FracESSA's exact safe candidate solver. The input game is Q=-A. For every accepted KKT
 * support, retain only its exact payoff; no candidate vector or second-order stability data is constructed.
 */
class candidate_search {
public:
    explicit candidate_search(const matrix_integer& matrix)
        : input_(matrix)
        , game_(matrix)
        , factorization_(matrix.rows())
        , progress_(progress::metric::bracelet, matrix.rows())
    {
        game_.negate();
        support_indices_.reserve(matrix.rows());
    }

    copositivity_classification classify(bool stop_at_zero)
    {
        fracessa_circular_detail::circular_support_generator generator(game_.rows());
        fracessa_circular_detail::circular_affine_symmetry symmetry(input_);
        bool candidate_found = false;
        bool decision_complete = false;
        size_t current_cardinality = 0;
        copositivity_classification result{true, true};
        generator.generate([&](const support& current_support, size_t support_size) {
            if (support_size != current_cardinality) {
                current_cardinality = support_size;
                progress_.stage(support_size);
            }
            progress_.visit_bracelet();
            if (!symmetry.is_representative(current_support)) {
                progress_.covered_support();
                return true;
            }
            current_support.copy_indices_to(support_indices_);
            if (support_size <= 3) {
                const copositivity_classification face =
                    small_copositivity::classify_principal(input_, support_indices_.data(), support_size);
                if (!face.is_copositive) {
                    result = {false, false};
                    decision_complete = true;
                    return false;
                }
                if (!face.is_strictly_copositive) {
                    result.is_strictly_copositive = false;
                    if (stop_at_zero) {
                        decision_complete = true;
                        return false;
                    }
                }
            }
            progress_.secondary();
            if (!find_candidate(current_support, support_size)) return true;
            progress_.split();
            candidate_found = true;
            if (payoff_numerator_.sign() > 0) {
                result = {false, false};
                decision_complete = true;
                return false;
            }
            if (payoff_numerator_.sign() == 0) {
                result.is_strictly_copositive = false;
                if (stop_at_zero) {
                    decision_complete = true;
                    return false;
                }
            }

            symmetry.for_each_distinct_bracelet_image(current_support, [&](const support& image) {
                generator.add_forbidden_orbit(image);
            });
            return true;
        });
        progress_.finish();
        if (!candidate_found && !decision_complete) throw std::runtime_error("fracessa_circular candidate search found no KKT point");
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
            if (current_support.contains(strategy)) continue;
            calculate_payoff(outside_payoff_numerator_, strategy, support_size);
            if (outside_payoff_numerator_.compare(payoff_numerator_) > 0) return false;
        }

        return true;
    }

    const matrix_integer& input_;
    matrix_integer game_;
    fraction_free_ldlt_factorization factorization_;
    std::vector<size_t> support_indices_;
    matrix_integer reduced_system_;
    matrix_integer solution_numerators_;
    integer solution_denominator_;
    integer reference_numerator_;
    integer payoff_numerator_;
    integer outside_payoff_numerator_;
    progress::tracker progress_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension <= 3) return small_copositivity::check(matrix, mode);

    const copositivity_classification result = candidate_search(matrix).classify(mode == copositivity_mode::strictly_copositive);
    return mode == copositivity_mode::copositive ? result.is_copositive : result.is_strictly_copositive;
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    if (matrix.rows() <= 3) return small_copositivity::classify(matrix);
    return candidate_search(matrix).classify(false);
}

} // namespace coposit::model
