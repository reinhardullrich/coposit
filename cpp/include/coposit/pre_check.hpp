#pragma once

#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/matrix_scan.hpp>
#include <coposit/matrix_integer.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>
#include <coposit/z_matrix_precheck.hpp>

#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace coposit::pre_check {

struct options {
    bool small_dimension = true;
    bool principal_submatrices = true;
    size_t principal_submatrices_up_to = 3;
    bool nonnegative_off_diagonal = true;
    bool negative_part_diagonal_dominance = true;
    bool all_ones = true;
    bool z_matrix = true;
    bool frank_wolfe = true;
    bool positive_definiteness = true;

    constexpr bool any() const noexcept
    {
        return small_dimension || principal_submatrices || nonnegative_off_diagonal
            || negative_part_diagonal_dominance || all_ones || z_matrix || frank_wolfe || positive_definiteness;
    }

    static constexpr options all() noexcept
    {
        return {true, true, 3, true, true, true, true, true, true};
    }

    static constexpr options none() noexcept
    {
        return {false, false, 3, false, false, false, false, false, false};
    }
};

namespace detail {

enum class query { copositive, strict, combined };

inline void validate_options(const options& selected)
{
    if (selected.principal_submatrices && (selected.principal_submatrices_up_to < 1 || selected.principal_submatrices_up_to > 3)) {
        throw std::invalid_argument("principal submatrix cutoff must be between 1 and 3");
    }
}

template<query requested>
matrix_scan_requirements requirements_for(const options& selected, bool force_negative_graph = false)
{
    const bool check_principal_pairs = selected.principal_submatrices && selected.principal_submatrices_up_to >= 2;
    matrix_scan_requirements requirements;
    requirements.negative_part_row_sums = selected.negative_part_diagonal_dominance;
    requirements.all_ones = selected.all_ones || selected.frank_wolfe;
    requirements.negative_graph = force_negative_graph || selected.z_matrix
        || (selected.principal_submatrices && selected.principal_submatrices_up_to >= 3);
    requirements.nonpositive_graph = selected.z_matrix;
    requirements.copositive_principal_pairs = check_principal_pairs && requested != query::strict;
    requirements.strict_principal_pairs = check_principal_pairs && requested != query::copositive;
    requirements.frank_wolfe = selected.frank_wolfe;
    requirements.motzkin_straus_pattern = selected.z_matrix;
    return requirements;
}

template<query requested>
matrix_scan_requirements preprocessing_requirements()
{
    matrix_scan_requirements requirements;
    requirements.negative_part_row_sums = true;
    requirements.all_ones = true;
    requirements.negative_graph = true;
    requirements.nonpositive_graph = true;
    requirements.copositive_principal_pairs = requested != query::strict;
    requirements.strict_principal_pairs = requested != query::copositive;
    requirements.frank_wolfe = true;
    requirements.off_diagonal_sign_counts = true;
    requirements.motzkin_straus_pattern = true;
    return requirements;
}

struct classification_state {
    void reject_copositive() noexcept
    {
        value = {false, false};
        copositive_known = true;
        strict_known = true;
    }

    void reject_strict() noexcept
    {
        value.is_strictly_copositive = false;
        strict_known = true;
    }

    void accept_copositive() noexcept
    {
        value.is_copositive = true;
        copositive_known = true;
    }

    void accept_strict() noexcept
    {
        value = {true, true};
        copositive_known = true;
        strict_known = true;
    }

    void merge(model::copositivity_classification result) noexcept
    {
        if (!copositive_known) {
            value.is_copositive = result.is_copositive;
            copositive_known = true;
        }
        if (!strict_known) {
            value.is_strictly_copositive = result.is_strictly_copositive;
            strict_known = true;
        }
        if (value.is_strictly_copositive) {
            value.is_copositive = true;
            copositive_known = true;
        }
        if (!value.is_copositive) {
            value.is_strictly_copositive = false;
            strict_known = true;
        }
    }

    void merge(const classification_state& result)
    {
        merge_fact(copositive_known, value.is_copositive, result.copositive_known, result.value.is_copositive);
        merge_fact(strict_known, value.is_strictly_copositive, result.strict_known, result.value.is_strictly_copositive);
        apply_implications();
    }

    void combine_by_and(const classification_state& result) noexcept
    {
        combine_fact_by_and(copositive_known, value.is_copositive, result.copositive_known, result.value.is_copositive);
        combine_fact_by_and(strict_known, value.is_strictly_copositive, result.strict_known, result.value.is_strictly_copositive);
        apply_implications();
    }

    template<query requested>
    bool done() const noexcept
    {
        if constexpr (requested == query::copositive) return copositive_known;
        if constexpr (requested == query::strict) return strict_known;
        return copositive_known && strict_known;
    }

    model::copositivity_classification value{false, false};
    bool copositive_known = false;
    bool strict_known = false;

private:
    static void merge_fact(bool& known, bool& current, bool result_known, bool result)
    {
        if (!result_known) return;
        if (!known) {
            known = true;
            current = result;
        } else if (current != result) {
            throw std::logic_error("conflicting exact preprocessing decisions");
        }
    }

    static void combine_fact_by_and(bool& known, bool& current, bool result_known, bool result) noexcept
    {
        if ((known && !current) || (result_known && !result)) {
            known = true;
            current = false;
        } else if (known && current && result_known && result) {
            known = true;
            current = true;
        } else {
            known = false;
            current = false;
        }
    }

    void apply_implications() noexcept
    {
        if (strict_known && value.is_strictly_copositive) {
            value.is_copositive = true;
            copositive_known = true;
        }
        if (copositive_known && !value.is_copositive) {
            value.is_strictly_copositive = false;
            strict_known = true;
        }
    }
};

inline void observe_nonpositive_value(classification_state& state, int sign) noexcept
{
    if (sign < 0) state.reject_copositive();
    else if (sign == 0) state.reject_strict();
}

inline void observe_positive_certificate(classification_state& state, bool copositive_passes, bool strict_passes) noexcept
{
    if (strict_passes) state.accept_strict();
    else if (copositive_passes) state.accept_copositive();
}

template<query requested>
void observe_small_face(classification_state& state, const matrix_integer& matrix, const size_t* indices, size_t dimension)
{
    if constexpr (requested == query::copositive) {
        if (!small_copositivity::check_principal<model::copositivity_mode::copositive>(matrix, indices, dimension)) {
            state.reject_copositive();
        }
    } else if constexpr (requested == query::strict) {
        if (!small_copositivity::check_principal<model::copositivity_mode::strictly_copositive>(matrix, indices, dimension)) {
            state.reject_strict();
        }
    } else {
        const model::copositivity_classification result = small_copositivity::classify_principal(matrix, indices, dimension);
        if (!result.is_copositive) state.reject_copositive();
        else if (!result.is_strictly_copositive) state.reject_strict();
    }
}

template<query requested>
model::copositivity_classification classify_small_matrix(const matrix_integer& matrix)
{
    if constexpr (requested == query::copositive) {
        const bool result = small_copositivity::check<model::copositivity_mode::copositive>(matrix);
        return {result, false};
    } else if constexpr (requested == query::strict) {
        const bool result = small_copositivity::check<model::copositivity_mode::strictly_copositive>(matrix);
        return {result, result};
    } else {
        return small_copositivity::classify(matrix);
    }
}

template<query requested>
classification_state classify_small_matrix_state(const matrix_integer& matrix)
{
    classification_state state;
    if constexpr (requested == query::copositive) {
        if (small_copositivity::check<model::copositivity_mode::copositive>(matrix)) state.accept_copositive();
        else state.reject_copositive();
    } else if constexpr (requested == query::strict) {
        if (small_copositivity::check<model::copositivity_mode::strictly_copositive>(matrix)) state.accept_strict();
        else state.reject_strict();
    } else {
        const model::copositivity_classification result = small_copositivity::classify(matrix);
        if (!result.is_copositive) state.reject_copositive();
        else if (result.is_strictly_copositive) state.accept_strict();
        else {
            state.accept_copositive();
            state.reject_strict();
        }
    }
    return state;
}

template<query requested>
void observe_small_principal_triples(classification_state& state, const matrix_integer& matrix,
                                     const std::vector<support>& negative_neighbors)
{
    std::vector<size_t> neighbors;
    neighbors.reserve(matrix.rows());

    for (size_t center = 0; center < matrix.rows(); ++center) {
        timeout_checkpoint();
        progress::advance_preprocessing(center + 1, matrix.rows());
        negative_neighbors[center].copy_indices_to(neighbors);
        for (size_t first = 0; first < neighbors.size(); ++first) {
            for (size_t second = first + 1; second < neighbors.size(); ++second) {
                timeout_checkpoint();
                const size_t left = neighbors[first];
                const size_t right = neighbors[second];
                const bool negative_triangle = matrix(left, right).sign() < 0;
                if (negative_triangle && (center > left || center > right)) continue;

                const size_t indices[] = {center, left, right};
                observe_small_face<requested>(state, matrix, indices, 3);
                if (state.done<requested>()) return;
            }
        }
    }
}

class frank_wolfe_witness_search {
public:
    template<typename Observer>
    void run(const matrix_integer& matrix, const matrix_scan_result& scan, Observer& observer)
    {
        if (scan.all_ones_quadratic_value.sign() <= 0 && observer(scan.all_ones_quadratic_value.sign())) return;
        initialize_centre(matrix, scan);

        double value = std::inner_product(x_.begin(), x_.end(), product_.begin(), 0.0);
        double best_value = value;
        best_x_ = x_;

        for (size_t iteration = 0; iteration < matrix.rows(); ++iteration) {
            timeout_checkpoint();
            progress::advance_preprocessing(iteration + 1, matrix.rows());
            if (!std::isfinite(value)) break;

            const size_t toward = static_cast<size_t>(std::min_element(product_.begin(), product_.end()) - product_.begin());
            const double minimum_product = product_[toward];
            const double gap = value - minimum_product;
            const double tolerance = numerical_tolerance(value, minimum_product);
            if (!(gap > tolerance)) break;

            const double curvature = diagonal_[toward] - 2.0 * minimum_product + value;
            const double alpha = curvature > 0.0 ? std::min(1.0, gap / curvature) : 1.0;
            if (!(alpha > 0.0) || !std::isfinite(alpha)) break;
            const double retained = 1.0 - alpha;
            if (retained == 1.0) break;

            const double previous_value = value;
            for (size_t coordinate = 0; coordinate < matrix.rows(); ++coordinate) {
                x_[coordinate] *= retained;
                product_[coordinate] = retained * product_[coordinate] + alpha * scaled(matrix(coordinate, toward));
            }
            x_[toward] += alpha;
            value = std::inner_product(x_.begin(), x_.end(), product_.begin(), 0.0);
            if (!std::isfinite(value)) break;

            if (value < best_value) {
                best_value = value;
                best_x_ = x_;
            }
            if (value <= numerical_tolerance(value, 0.0) && observe_if_nonpositive(matrix, x_, observer)) return;
            if (value == previous_value) break;
        }

        static_cast<void>(observe_if_nonpositive(matrix, best_x_, observer));
    }

private:
    static constexpr double quantization_scale = 1099511627776.0; // 2^40

    static double numerical_tolerance(double left, double right) noexcept
    {
        return 64.0 * std::numeric_limits<double>::epsilon()
            * std::max({1.0, std::abs(left), std::abs(right)});
    }

    void initialize_centre(const matrix_integer& matrix, const matrix_scan_result& scan)
    {
        maximum_exponent_ = 0;
        if (!scan.maximum_absolute_entry.is_zero()) {
            static_cast<void>(scan.maximum_absolute_entry.to_dbl_2exp(maximum_exponent_));
        }

        const double inverse_dimension = 1.0 / static_cast<double>(matrix.rows());
        x_.assign(matrix.rows(), inverse_dimension);
        product_.resize(matrix.rows());
        diagonal_.resize(matrix.rows());
        for (size_t row = 0; row < matrix.rows(); ++row) {
            product_[row] = scaled(scan.full_row_sums[row]) * inverse_dimension;
            diagonal_[row] = scaled(matrix(row, row));
        }
    }

    double scaled(integer::const_reference value) const noexcept
    {
        if (value.is_zero()) return 0.0;
        slong exponent = 0;
        const double mantissa = value.to_dbl_2exp(exponent);
        const slong difference = exponent - maximum_exponent_;
        if (difference < std::numeric_limits<int>::min()) return 0.0;
        return std::scalbn(mantissa, static_cast<int>(difference));
    }

    template<typename Observer>
    bool observe_if_nonpositive(const matrix_integer& matrix, const std::vector<double>& candidate, Observer& observer)
    {
        const int sign = exact_witness_sign(matrix, candidate);
        return sign <= 0 && observer(sign);
    }

    int exact_witness_sign(const matrix_integer& matrix, const std::vector<double>& candidate)
    {
        exact_weights_.resize(candidate.size());
        active_.clear();
        size_t largest = 0;
        for (size_t coordinate = 0; coordinate < candidate.size(); ++coordinate) {
            if (candidate[coordinate] > candidate[largest]) largest = coordinate;
            const double value = std::max(0.0, candidate[coordinate]);
            const slong weight = std::isfinite(value) ? static_cast<slong>(std::llround(value * quantization_scale)) : 0;
            exact_weights_[coordinate] = integer(weight);
            if (weight != 0) active_.push_back(coordinate);
        }
        if (active_.empty()) {
            exact_weights_[largest].set_one();
            active_.push_back(largest);
        }

        integer quadratic;
        for (const size_t row : active_) {
            timeout_checkpoint();
            row_product_.set_zero();
            for (const size_t column : active_) row_product_.addmul(matrix(row, column), exact_weights_[column]);
            quadratic.addmul(exact_weights_[row], row_product_);
        }
        return quadratic.sign();
    }

    std::vector<double> x_;
    std::vector<double> product_;
    std::vector<double> diagonal_;
    std::vector<double> best_x_;
    std::vector<integer> exact_weights_;
    std::vector<size_t> active_;
    integer row_product_;
    slong maximum_exponent_ = 0;
};

template<query requested>
classification_state root_checks_scanned(const matrix_integer& matrix, const matrix_scan_result& scan)
{
    const size_t dimension = matrix.rows();
    if (scan.dimension != dimension) throw std::logic_error("matrix scan dimension does not match matrix");

    progress::preprocessing_stage(progress::preprocessing_phase::root_checks, dimension);
    if (dimension <= 3) return classify_small_matrix_state<requested>(matrix);

    classification_state state;
    if (!scan.all_diagonals_nonnegative) state.reject_copositive();
    else if (!scan.all_diagonals_positive) state.reject_strict();

    if (!state.done<requested>()) {
        if constexpr (requested == query::copositive) {
            if (!scan.all_principal_pairs_copositive) state.reject_copositive();
        } else if constexpr (requested == query::strict) {
            if (!scan.all_principal_pairs_strictly_copositive) state.reject_strict();
        } else {
            if (!scan.all_principal_pairs_copositive) state.reject_copositive();
            else if (!scan.all_principal_pairs_strictly_copositive) state.reject_strict();
        }
    }

    if (!state.done<requested>() && !scan.has_negative_off_diagonal) {
        if (scan.all_diagonals_positive) state.accept_strict();
        else if (scan.all_diagonals_nonnegative) {
            state.accept_copositive();
            state.reject_strict();
        }
    }
    return state;
}

template<query requested>
classification_state ordinary_checks_scanned(const matrix_integer& matrix, const matrix_scan_result& scan)
{
    const size_t dimension = matrix.rows();
    if (scan.dimension != dimension) throw std::logic_error("matrix scan dimension does not match matrix");
    progress::preprocessing_stage(progress::preprocessing_phase::principal_submatrices, dimension, 0, dimension);
    if (dimension <= 3) return classify_small_matrix_state<requested>(matrix);

    classification_state state;
    observe_small_principal_triples<requested>(state, matrix, scan.negative_neighbors);
    if (state.done<requested>()) return state;

    progress::preprocessing_stage(progress::preprocessing_phase::negative_part_diagonal_dominance, dimension);
    bool all_row_sums_nonnegative = true;
    bool all_row_sums_positive = true;
    for (const integer& row_sum : scan.negative_part_row_sums) {
        all_row_sums_nonnegative &= row_sum.sign() >= 0;
        all_row_sums_positive &= row_sum.sign() > 0;
    }
    observe_positive_certificate(state, all_row_sums_nonnegative, all_row_sums_positive);
    if (state.done<requested>()) return state;

    progress::preprocessing_stage(progress::preprocessing_phase::all_ones, dimension);
    observe_nonpositive_value(state, scan.all_ones_quadratic_value.sign());
    if (state.done<requested>()) return state;

    progress::preprocessing_stage(progress::preprocessing_phase::frank_wolfe, dimension, 0, dimension);
    frank_wolfe_witness_search search;
    auto observer = [&](int sign) {
        observe_nonpositive_value(state, sign);
        return state.done<requested>();
    };
    search.run(matrix, scan, observer);
    if (state.done<requested>()) return state;

    z_matrix_precheck::request z_request = z_matrix_precheck::request::combined;
    if constexpr (requested == query::copositive) z_request = z_matrix_precheck::request::copositive;
    else if constexpr (requested == query::strict) z_request = z_matrix_precheck::request::strict;
    const z_matrix_precheck::outcome z_result = z_matrix_precheck::check(
        matrix, scan.negative_neighbors, scan.nonpositive_neighbors, scan.is_motzkin_straus_pattern, z_request);
    if (z_result == z_matrix_precheck::outcome::not_copositive) state.reject_copositive();
    else if (z_result == z_matrix_precheck::outcome::not_strictly_copositive) state.reject_strict();
    if (state.done<requested>()) return state;

    progress::preprocessing_stage(progress::preprocessing_phase::exact_factorization, dimension, 0, dimension);
    matrix_integer factored(matrix);
    fraction_free_ldlt_factorization factorization(dimension);
    factorization.factorize_inplace(factored, progress::enabled());
    const bool positive_semidefinite = factorization.is_positive_semidefinite();
    const bool positive_definite = factorization.is_positive_definite();
    observe_positive_certificate(state, positive_semidefinite, positive_definite);
    if (state.done<requested>()) return state;

    if constexpr (requested != query::copositive) {
        if (positive_semidefinite && !positive_definite && dimension - factorization.rank() == 1) {
            matrix_integer kernel_vector(dimension, 1);
            factorization.one_nullspace_vector(kernel_vector, factored);
            bool has_positive = false;
            bool has_negative = false;
            for (size_t row = 0; row < dimension; ++row) {
                timeout_checkpoint();
                has_positive |= kernel_vector(row, 0).sign() > 0;
                has_negative |= kernel_vector(row, 0).sign() < 0;
            }
            assert(has_positive || has_negative);
            if (has_positive && has_negative) state.accept_strict();
            else state.reject_strict();
        }
    }
    return state;
}

template<query requested, typename FinalClassifier>
model::copositivity_classification run_scanned(const matrix_integer& matrix, const options& selected,
                                               const matrix_scan_result& scan, FinalClassifier& final_classifier)
{
    const size_t dimension = matrix.rows();
    if (scan.dimension != dimension) throw std::logic_error("matrix scan dimension does not match matrix");

    const bool check_small_principals = selected.principal_submatrices && dimension > 3;
    const bool check_principal_pairs = check_small_principals && selected.principal_submatrices_up_to >= 2;
    const bool check_principal_triples = check_small_principals && selected.principal_submatrices_up_to >= 3;

    progress::preprocessing_stage(progress::preprocessing_phase::cheap_certificates, dimension);
    if (selected.small_dimension && dimension <= 3) {
        return classify_small_matrix<requested>(matrix);
    }

    classification_state state;
    if (check_small_principals) {
        if (!scan.all_diagonals_nonnegative) state.reject_copositive();
        else if (!scan.all_diagonals_positive) state.reject_strict();

        if (check_principal_pairs && !state.done<requested>()) {
            if constexpr (requested == query::copositive) {
                if (!scan.all_principal_pairs_copositive) state.reject_copositive();
            } else if constexpr (requested == query::strict) {
                if (!scan.all_principal_pairs_strictly_copositive) state.reject_strict();
            } else {
                if (!scan.all_principal_pairs_copositive) state.reject_copositive();
                else if (!scan.all_principal_pairs_strictly_copositive) state.reject_strict();
            }
        }
    }

    if (state.done<requested>()) return state.value;
    if (selected.nonnegative_off_diagonal && !scan.has_negative_off_diagonal) {
        if (scan.all_diagonals_positive) {
            state.accept_strict();
        } else if (scan.all_diagonals_nonnegative) {
            state.accept_copositive();
            state.reject_strict();
        }
        if (state.done<requested>()) return state.value;
    }

    if (selected.negative_part_diagonal_dominance) {
        bool all_row_sums_nonnegative = true;
        bool all_row_sums_positive = true;
        for (const integer& row_sum : scan.negative_part_row_sums) {
            all_row_sums_nonnegative &= row_sum.sign() >= 0;
            all_row_sums_positive &= row_sum.sign() > 0;
        }
        observe_positive_certificate(state, all_row_sums_nonnegative, all_row_sums_positive);
        if (state.done<requested>()) return state.value;
    }

    if (selected.all_ones) {
        observe_nonpositive_value(state, scan.all_ones_quadratic_value.sign());
        if (state.done<requested>()) return state.value;
    }

    if (check_principal_triples) {
        progress::preprocessing_stage(progress::preprocessing_phase::principal_submatrices, dimension, 0, dimension);
        observe_small_principal_triples<requested>(state, matrix, scan.negative_neighbors);
        if (state.done<requested>()) return state.value;
    }

    if (selected.z_matrix) {
        z_matrix_precheck::request z_request = z_matrix_precheck::request::combined;
        if constexpr (requested == query::copositive) z_request = z_matrix_precheck::request::copositive;
        else if constexpr (requested == query::strict) z_request = z_matrix_precheck::request::strict;

        const z_matrix_precheck::outcome z_result = z_matrix_precheck::check(
            matrix, scan.negative_neighbors, scan.nonpositive_neighbors, scan.is_motzkin_straus_pattern, z_request);
        if (z_result == z_matrix_precheck::outcome::not_copositive) state.reject_copositive();
        else if (z_result == z_matrix_precheck::outcome::not_strictly_copositive) state.reject_strict();
        if (state.done<requested>()) return state.value;
    }

    if (selected.frank_wolfe) {
        progress::preprocessing_stage(progress::preprocessing_phase::frank_wolfe, dimension, 0, dimension);
        frank_wolfe_witness_search search;
        auto observer = [&](int sign) {
            observe_nonpositive_value(state, sign);
            return state.done<requested>();
        };
        search.run(matrix, scan, observer);
        if (state.done<requested>()) return state.value;
    }

    if (selected.positive_definiteness) {
        progress::preprocessing_stage(progress::preprocessing_phase::exact_factorization, dimension, 0, dimension);
        matrix_integer factored(matrix);
        fraction_free_ldlt_factorization factorization(dimension);
        factorization.factorize_inplace(factored, progress::enabled());
        const bool positive_semidefinite = factorization.is_positive_semidefinite();
        const bool positive_definite = factorization.is_positive_definite();
        observe_positive_certificate(state, positive_semidefinite, positive_definite);
        if (state.done<requested>()) return state.value;

        if constexpr (requested != query::copositive) {
            if (positive_semidefinite && !positive_definite && dimension - factorization.rank() == 1) {
                matrix_integer kernel_vector(dimension, 1);
                factorization.one_nullspace_vector(kernel_vector, factored);
                bool has_positive = false;
                bool has_negative = false;
                for (size_t row = 0; row < dimension; ++row) {
                    timeout_checkpoint();
                    has_positive |= kernel_vector(row, 0).sign() > 0;
                    has_negative |= kernel_vector(row, 0).sign() < 0;
                }
                assert(has_positive || has_negative);
                // One vector spans a nullity-one kernel. Mixed signs exclude a nonzero nonnegative multiple; either one-sided
                // orientation is itself a nonnegative zero of the quadratic form.
                if (has_positive && has_negative) state.accept_strict();
                else state.reject_strict();
                if (state.done<requested>()) return state.value;
            }
        }
    }

    state.merge(final_classifier(matrix));
    return state.value;
}

template<query requested, typename FinalClassifier>
model::copositivity_classification run(const matrix_integer& matrix, const options& selected,
                                       FinalClassifier& final_classifier)
{
    validate_options(selected);
    progress::preprocessing_stage(progress::preprocessing_phase::matrix_scan, matrix.rows(), 0, matrix.rows());
    const matrix_scan_result scan = scan_matrix(matrix, requirements_for<requested>(selected));
    return run_scanned<requested>(matrix, selected, scan, final_classifier);
}

} // namespace detail

/*
 * Individually selectable exact decisions before a final non-strict- or strict-copositivity algorithm. All options default to true.
 * With options::none(), the callback is invoked directly and retains responsibility for validating the input.
 */
template<typename FinalAlgorithm>
bool check(const matrix_integer& matrix, model::copositivity_mode mode, const options& selected, FinalAlgorithm&& final_algorithm)
{
    if (!selected.any()) {
        progress::preprocessing_stage(progress::preprocessing_phase::model_delegation, matrix.rows());
        return final_algorithm(matrix);
    }
    if (mode == model::copositivity_mode::strictly_copositive) {
        auto classifier = [&](const matrix_integer& part) {
            progress::preprocessing_stage(progress::preprocessing_phase::model_delegation, part.rows());
            const bool result = final_algorithm(part);
            return model::copositivity_classification{result, result};
        };
        return detail::run<detail::query::strict>(matrix, selected, classifier).is_strictly_copositive;
    }

    auto classifier = [&](const matrix_integer& part) {
        progress::preprocessing_stage(progress::preprocessing_phase::model_delegation, part.rows());
        const bool result = final_algorithm(part);
        return model::copositivity_classification{result, false};
    };
    return detail::run<detail::query::copositive>(matrix, selected, classifier).is_copositive;
}

/* Run selected pre-checks once and preserve the non-strict/strict equality distinction supplied by a combined final classifier. */
template<typename FinalClassifier>
model::copositivity_classification classify(const matrix_integer& matrix, const options& selected,
                                            FinalClassifier&& final_classifier)
{
    auto classifier = [&](const matrix_integer& part) {
        progress::preprocessing_stage(progress::preprocessing_phase::model_delegation, part.rows());
        return final_classifier(part);
    };
    if (!selected.any()) return classifier(matrix);
    return detail::run<detail::query::combined>(matrix, selected, classifier);
}

} // namespace coposit::pre_check
