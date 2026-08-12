#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

struct certificate_signature {
    explicit certificate_signature(size_t dimension)
        : nonzero_support(dimension)
        , product_nonnegative(dimension)
    {
    }

    support nonzero_support;
    support product_nonnegative;
};

#ifdef COPOSIT_FRANK_WOLFE_DICKINSON_TESTING
bool last_frank_wolfe_witness = false;
bool last_iterative_frank_wolfe_witness = false;
#endif

class frank_wolfe_witness_search {
public:
    bool find(const matrix_integer& matrix)
    {
        if (find_scale_and_check_direct_witness(matrix)) return true;

        static_cast<void>(maximum_.to_dbl_2exp(maximum_exponent_));
        const double inverse_dimension = 1.0 / static_cast<double>(matrix.rows());
        x_.assign(matrix.rows(), inverse_dimension);
        product_.assign(matrix.rows(), 0.0);
        diagonal_.resize(matrix.rows());
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column) {
                const double value = scaled(matrix(row, column));
                product_[row] += value;
                if (row == column) diagonal_[row] = value;
                else product_[column] += value;
            }
        }
        for (double& value : product_) value *= inverse_dimension;

        const search_result uniform_result = run(matrix);
        if (uniform_result.exact_witness) {
            iterative_witness_ = true;
            return true;
        }
        double best_value = uniform_result.value;
        best_x_ = x_;

        std::vector<size_t> seeds(matrix.rows());
        std::iota(seeds.begin(), seeds.end(), 0);
        const size_t seed_count = std::min(maximum_vertex_starts, matrix.rows());
        std::partial_sort(seeds.begin(), seeds.begin() + seed_count, seeds.end(), [&](size_t left, size_t right) {
            const double left_diagonal = diagonal_[left];
            const double right_diagonal = diagonal_[right];
            return left_diagonal < right_diagonal || (left_diagonal == right_diagonal && left < right);
        });

        for (size_t seed = 0; seed < seed_count; ++seed) {
            timeout_checkpoint();
            const size_t vertex = seeds[seed];
            std::fill(x_.begin(), x_.end(), 0.0);
            x_[vertex] = 1.0;
            for (size_t row = 0; row < matrix.rows(); ++row) product_[row] = scaled(matrix(row, vertex));

            const search_result result = run(matrix);
            if (result.exact_witness) {
                iterative_witness_ = true;
                return true;
            }
            if (result.value < best_value) {
                best_value = result.value;
                best_x_ = x_;
            }
        }

        iterative_witness_ = best_value <= 0.0 && exact_nonpositive(matrix, best_x_);
        return iterative_witness_;
    }

    bool used_iterative_witness() const noexcept { return iterative_witness_; }

private:
    struct search_result {
        double value;
        bool exact_witness;
    };

    static constexpr size_t maximum_iterations = 64;
    static constexpr size_t maximum_vertex_starts = 7;
    static constexpr double clear_negative_threshold = -1.0e-12;
    static constexpr double quantization_scale = 1099511627776.0; // 2^40

    bool find_scale_and_check_direct_witness(const matrix_integer& matrix)
    {
        integer quadratic;
        maximum_.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            if (matrix(row, row).sign() <= 0) return true;
            for (size_t column = 0; column <= row; ++column) {
                const auto entry = matrix(row, column);
                quadratic += entry;
                if (row != column) quadratic += entry;
                if (entry.compare_abs(maximum_) > 0) maximum_.set_abs(entry);
            }
        }
        return quadratic.sign() <= 0;
    }

    double scaled(integer::const_reference value) const
    {
        if (value.is_zero()) return 0.0;
        slong exponent = 0;
        const double mantissa = value.to_dbl_2exp(exponent);
        const slong difference = exponent - maximum_exponent_;
        if (difference < std::numeric_limits<int>::min()) return 0.0;
        return std::scalbn(mantissa, static_cast<int>(difference));
    }

    search_result run(const matrix_integer& matrix)
    {
        double value = std::inner_product(x_.begin(), x_.end(), product_.begin(), 0.0);
        bool tried_clear_negative = false;

        for (size_t iteration = 0; iteration < maximum_iterations; ++iteration) {
            timeout_checkpoint();
            if (!std::isfinite(value)) break;
            if (value < clear_negative_threshold && !tried_clear_negative) {
                tried_clear_negative = true;
                if (exact_nonpositive(matrix, x_)) return {value, true};
            }

            const size_t vertex = static_cast<size_t>(std::min_element(product_.begin(), product_.end()) - product_.begin());
            const double slope = product_[vertex] - value;
            if (!(slope < 0.0)) break;

            const double curvature = diagonal_[vertex] - 2.0 * product_[vertex] + value;
            const double alpha = curvature > 0.0 ? std::min(1.0, -slope / curvature) : 1.0;
            if (!(alpha > 0.0) || !std::isfinite(alpha)) break;

            const double retained = 1.0 - alpha;
            for (size_t coordinate = 0; coordinate < matrix.rows(); ++coordinate) {
                x_[coordinate] *= retained;
                product_[coordinate] = retained * product_[coordinate] + alpha * scaled(matrix(coordinate, vertex));
            }
            x_[vertex] += alpha;
            value = std::inner_product(x_.begin(), x_.end(), product_.begin(), 0.0);
        }

        return {value, false};
    }

    bool exact_nonpositive(const matrix_integer& matrix, const std::vector<double>& candidate)
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
        return quadratic.sign() <= 0;
    }

    std::vector<double> x_;
    std::vector<double> product_;
    std::vector<double> diagonal_;
    std::vector<double> best_x_;
    std::vector<integer> exact_weights_;
    std::vector<size_t> active_;
    integer maximum_;
    integer row_product_;
    slong maximum_exponent_ = 0;
    bool iterative_witness_ = false;
};

class dickinson_checker {
public:
    explicit dickinson_checker(size_t dimension)
        : factorization_(dimension)
        , product_(dimension)
        , certificates_by_lowest_(dimension)
    {
    }

    bool check(const matrix_integer& matrix)
    {
        const size_t matrix_dimension = matrix.rows();
        for (size_t subset_dimension = 1; subset_dimension <= matrix_dimension; ++subset_dimension) {
            std::vector<size_t> indices(subset_dimension);
            support current_support(matrix_dimension);
            for (size_t i = 0; i < subset_dimension; ++i) {
                indices[i] = i;
                current_support.set(i);
            }

            do {
                timeout_checkpoint();
                if (subset_dimension <= 3
                    && !small_copositivity::check_principal(matrix, indices.data(), subset_dimension)) return false;
                if (!is_covered(current_support, indices) && !process_subset(matrix, indices)) return false;
            } while (advance_numeric_mask_order(indices, current_support, matrix_dimension));
        }

        return true;
    }

private:
    static bool advance_numeric_mask_order(std::vector<size_t>& indices, support& current_support, size_t matrix_dimension)
    {
        for (size_t i = 0; i < indices.size(); ++i) {
            const size_t upper_bound = i + 1 < indices.size() ? indices[i + 1] : matrix_dimension;
            if (indices[i] + 1 == upper_bound) continue;

            current_support.reset(indices[i]);
            ++indices[i];
            current_support.set(indices[i]);
            for (size_t reset = 0; reset < i; ++reset) {
                current_support.reset(indices[reset]);
                indices[reset] = reset;
                current_support.set(indices[reset]);
            }
            return true;
        }
        return false;
    }

    bool is_covered(const support& current_support, const std::vector<size_t>& indices) const
    {
        for (const size_t lowest : indices) {
            const std::vector<certificate_signature>& certificates = certificates_by_lowest_[lowest];
            for (auto certificate = certificates.rbegin(); certificate != certificates.rend(); ++certificate) {
                timeout_checkpoint();
                if (certificate->nonzero_support.is_subset_of(current_support)
                    && current_support.is_subset_of(certificate->product_nonnegative)) return true;
            }
        }
        return false;
    }

    bool process_subset(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        const size_t dimension = indices.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices, principal_);

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
        if (all_nonnegative) return false;

        add_certificate(matrix, indices, solution_);
        return true;
    }

    void add_certificate(const matrix_integer& matrix, const std::vector<size_t>& indices, const matrix_integer& solution)
    {
        certificate_signature certificate(matrix.rows());
        for (size_t local = 0; local < indices.size(); ++local) {
            if (!solution(local, 0).is_zero()) certificate.nonzero_support.set(indices[local]);
        }

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices.size(); ++local) {
                product_[row].addmul(matrix(row, indices[local]), solution(local, 0));
            }
            if (product_[row].sign() >= 0) certificate.product_nonnegative.set(row);
        }

        certificates_by_lowest_[certificate.nonzero_support.lowest_index()].push_back(std::move(certificate));
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column) principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    std::vector<integer> product_;
    std::vector<std::vector<certificate_signature>> certificates_by_lowest_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    require_strict_mode(mode);
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
#ifdef COPOSIT_FRANK_WOLFE_DICKINSON_TESTING
    last_frank_wolfe_witness = false;
    last_iterative_frank_wolfe_witness = false;
#endif
    if (dimension <= 3) return small_copositivity::check(matrix);

    frank_wolfe_witness_search witness_search;
    if (witness_search.find(matrix)) {
#ifdef COPOSIT_FRANK_WOLFE_DICKINSON_TESTING
        last_frank_wolfe_witness = true;
        last_iterative_frank_wolfe_witness = witness_search.used_iterative_witness();
#endif
        return false;
    }
    return dickinson_checker(dimension).check(matrix);
}

#ifdef COPOSIT_FRANK_WOLFE_DICKINSON_TESTING
bool frank_wolfe_witness_found_for_testing() noexcept
{
    return last_frank_wolfe_witness;
}

bool iterative_frank_wolfe_witness_found_for_testing() noexcept
{
    return last_iterative_frank_wolfe_witness;
}
#endif

} // namespace coposit::model
