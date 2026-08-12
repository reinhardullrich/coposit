#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
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

struct positive_ratio {
    integer numerator;
    integer denominator;
};

bool ratio_less(const positive_ratio& left, const positive_ratio& right)
{
    integer left_product;
    integer right_product;
    left_product.set_product(left.numerator, right.denominator);
    right_product.set_product(right.numerator, left.denominator);
    return left_product.compare(right_product) < 0;
}

bool ratio_equal(const positive_ratio& left, const positive_ratio& right)
{
    return !ratio_less(left, right) && !ratio_less(right, left);
}

#ifdef COPOSIT_RHS_DICKINSON_TESTING
size_t last_rhs_optimized_certificate_count = 0;
#endif

class rhs_dickinson_checker {
public:
    explicit rhs_dickinson_checker(size_t dimension)
        : factorization_(dimension)
        , base_product_(dimension)
        , direction_product_(dimension)
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

#ifdef COPOSIT_RHS_DICKINSON_TESTING
    size_t rhs_optimized_certificate_count() const noexcept { return rhs_optimized_certificate_count_; }
#endif

private:
    struct coverage_score {
        bool admissible = false;
        size_t width = 0;
        size_t upper_size = 0;
    };

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

        if (factorization_.factorize_inplace(principal_) != 0) return process_nonsingular_subset(matrix, indices);

        factorization_.one_nullspace_vector(solution_, principal_);
        bool has_positive_entry = false;
        for (size_t row = 0; row < dimension; ++row) has_positive_entry |= solution_(row, 0).sign() > 0;
        if (!has_positive_entry) solution_.negate();

        bool all_nonnegative = true;
        for (size_t row = 0; row < dimension; ++row) all_nonnegative &= solution_(row, 0).sign() >= 0;
        if (all_nonnegative) return false;

        return add_certificate(matrix, indices, solution_);
    }

    bool process_nonsingular_subset(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        const size_t dimension = indices.size();
        for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);

        bool all_nonpositive = true;
        for (size_t row = 0; row < dimension; ++row) all_nonpositive &= solution_(row, 0).sign() <= 0;
        if (all_nonpositive) return false;

        calculate_product(matrix, indices, solution_, 0, base_product_);
        const coverage_score baseline = score_baseline();
        best_width_ = baseline.width;
        best_upper_size_ = baseline.upper_size;
        best_direction_ = dimension;
        best_numerator_.set_zero();
        best_denominator_.set_one();

        if (dimension > 1) {
            directions_.resize(dimension, dimension);
            for (size_t row = 0; row < dimension; ++row) {
                for (size_t column = 0; column < dimension; ++column) {
                    if (row == column) directions_(row, column).set_one();
                    else directions_(row, column).set_zero();
                }
            }

            integer direction_denominator;
            factorization_.solve_inplace(directions_, direction_denominator, principal_);
            assert(direction_denominator.compare(denominator) == 0);

            for (size_t direction = 0; direction < dimension; ++direction) {
                calculate_product(matrix, indices, directions_, direction, direction_product_);
                find_breakpoints(direction);
                sweep_direction(direction);
            }
        }

        if (best_direction_ != dimension) {
            for (size_t row = 0; row < dimension; ++row) {
                set_linear_combination(solution_entry_, solution_(row, 0), directions_(row, best_direction_),
                                       best_numerator_, best_denominator_);
                solution_(row, 0) = solution_entry_;
            }
#ifdef COPOSIT_RHS_DICKINSON_TESTING
            ++rhs_optimized_certificate_count_;
#endif
        }

        return add_certificate(matrix, indices, solution_);
    }

    coverage_score score_baseline() const
    {
        coverage_score score;
        size_t lower_size = 0;
        for (size_t row = 0; row < solution_.rows(); ++row) {
            score.admissible |= solution_(row, 0).sign() > 0;
            lower_size += !solution_(row, 0).is_zero();
        }
        for (const integer& value : base_product_) score.upper_size += value.sign() >= 0;
        assert(score.upper_size >= lower_size);
        score.width = score.upper_size - lower_size;
        return score;
    }

    coverage_score score_candidate(size_t direction, const integer& numerator, const integer& denominator)
    {
        coverage_score score;
        size_t lower_size = 0;
        for (size_t row = 0; row < solution_.rows(); ++row) {
            set_linear_combination(solution_entry_, solution_(row, 0), directions_(row, direction), numerator, denominator);
            score.admissible |= solution_entry_.sign() > 0;
            lower_size += !solution_entry_.is_zero();
        }
        if (!score.admissible) return score;

        for (size_t row = 0; row < base_product_.size(); ++row) {
            set_linear_combination(product_entry_, base_product_[row], direction_product_[row], numerator, denominator);
            score.upper_size += product_entry_.sign() >= 0;
        }
        assert(score.upper_size >= lower_size);
        score.width = score.upper_size - lower_size;
        return score;
    }

    void find_breakpoints(size_t direction)
    {
        breakpoints_.clear();
        for (size_t row = 0; row < solution_.rows(); ++row) {
            add_positive_breakpoint(solution_(row, 0), directions_(row, direction));
        }
        for (size_t row = 0; row < base_product_.size(); ++row) {
            add_positive_breakpoint(base_product_[row], direction_product_[row]);
        }

        std::sort(breakpoints_.begin(), breakpoints_.end(), ratio_less);
        breakpoints_.erase(std::unique(breakpoints_.begin(), breakpoints_.end(), ratio_equal), breakpoints_.end());
    }

    void add_positive_breakpoint(integer::const_reference base, integer::const_reference direction)
    {
        if (base.is_zero() || direction.is_zero() || base.sign() == direction.sign()) return;
        positive_ratio breakpoint;
        breakpoint.numerator.set_abs(base);
        breakpoint.denominator.set_abs(direction);
        breakpoints_.push_back(std::move(breakpoint));
    }

    void sweep_direction(size_t direction)
    {
        if (breakpoints_.empty()) return;

        positive_ratio sample;
        sample.numerator = breakpoints_.front().numerator;
        sample.denominator = breakpoints_.front().denominator;
        sample.denominator.multiply(2);
        consider_candidate(direction, sample);

        for (size_t index = 0; index < breakpoints_.size(); ++index) {
            consider_candidate(direction, breakpoints_[index]);
            if (index + 1 == breakpoints_.size()) continue;

            midpoint(sample, breakpoints_[index], breakpoints_[index + 1]);
            consider_candidate(direction, sample);
        }

        sample.numerator = breakpoints_.back().numerator;
        sample.numerator += breakpoints_.back().denominator;
        sample.denominator = breakpoints_.back().denominator;
        consider_candidate(direction, sample);
    }

    static void midpoint(positive_ratio& result, const positive_ratio& left, const positive_ratio& right)
    {
        integer second_term;
        result.numerator.set_product(left.numerator, right.denominator);
        second_term.set_product(right.numerator, left.denominator);
        result.numerator += second_term;
        result.denominator.set_product(left.denominator, right.denominator);
        result.denominator.multiply(2);
    }

    void consider_candidate(size_t direction, const positive_ratio& candidate)
    {
        const coverage_score score = score_candidate(direction, candidate.numerator, candidate.denominator);
        if (!score.admissible) return;
        if (score.width < best_width_ || (score.width == best_width_ && score.upper_size <= best_upper_size_)) return;

        best_width_ = score.width;
        best_upper_size_ = score.upper_size;
        best_direction_ = direction;
        best_numerator_ = candidate.numerator;
        best_denominator_ = candidate.denominator;
    }

    static void set_linear_combination(integer& result, integer::const_reference base, integer::const_reference direction,
                                       integer::const_reference numerator, integer::const_reference denominator)
    {
        result.set_product(base, denominator);
        result.addmul(direction, numerator);
    }

    static void calculate_product(const matrix_integer& matrix, const std::vector<size_t>& indices,
                                  const matrix_integer& vectors, size_t column, std::vector<integer>& product)
    {
        for (integer& value : product) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices.size(); ++local) {
                product[row].addmul(matrix(row, indices[local]), vectors(local, column));
            }
        }
    }

    bool add_certificate(const matrix_integer& matrix, const std::vector<size_t>& indices, const matrix_integer& solution)
    {
        certificate_signature certificate(matrix.rows());
        for (size_t local = 0; local < indices.size(); ++local) {
            if (!solution(local, 0).is_zero()) certificate.nonzero_support.set(indices[local]);
        }

        calculate_product(matrix, indices, solution, 0, product_);
        for (size_t row = 0; row < matrix.rows(); ++row) {
            if (product_[row].sign() >= 0) certificate.product_nonnegative.set(row);
        }

        bool solution_nonnegative = true;
        integer quadratic;
        for (size_t local = 0; local < indices.size(); ++local) {
            solution_nonnegative &= solution(local, 0).sign() >= 0;
            quadratic.addmul(solution(local, 0), product_[indices[local]]);
        }
        if (solution_nonnegative && quadratic.is_zero()) return false;
        certificates_by_lowest_[certificate.nonzero_support.lowest_index()].push_back(std::move(certificate));
        return true;
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column) {
                principal(row, column) = matrix(indices[row], indices[column]);
            }
        }
    }

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    matrix_integer directions_;
    std::vector<integer> base_product_;
    std::vector<integer> direction_product_;
    std::vector<integer> product_;
    std::vector<std::vector<certificate_signature>> certificates_by_lowest_;
    std::vector<positive_ratio> breakpoints_;
    integer solution_entry_;
    integer product_entry_;
    integer best_numerator_;
    integer best_denominator_;
    size_t best_width_ = 0;
    size_t best_upper_size_ = 0;
    size_t best_direction_ = 0;
#ifdef COPOSIT_RHS_DICKINSON_TESTING
    size_t rhs_optimized_certificate_count_ = 0;
#endif
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    require_strict_mode(mode);
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
#ifdef COPOSIT_RHS_DICKINSON_TESTING
    last_rhs_optimized_certificate_count = 0;
#endif
    if (dimension <= 3) return small_copositivity::check(matrix);

    rhs_dickinson_checker checker(dimension);
    const bool result = checker.check(matrix);
#ifdef COPOSIT_RHS_DICKINSON_TESTING
    last_rhs_optimized_certificate_count = checker.rhs_optimized_certificate_count();
#endif
    return result;
}

#ifdef COPOSIT_RHS_DICKINSON_TESTING
size_t rhs_dickinson_optimized_certificate_count_for_testing() noexcept
{
    return last_rhs_optimized_certificate_count;
}
#endif

} // namespace coposit::model
