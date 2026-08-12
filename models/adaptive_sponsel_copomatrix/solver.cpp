#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/progress.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/timeout.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

constexpr size_t sponsel_streak_limit = 1000;

struct sparse_ray {
    std::array<size_t, 2> indices{};
    std::array<integer, 2> coefficients{};
    size_t count = 1;
};

struct positive_ratio {
    integer numerator;
    integer denominator;
};

int compare(const positive_ratio& left, const positive_ratio& right)
{
    integer cross_left;
    integer cross_right;
    cross_left.set_product(left.numerator, right.denominator);
    cross_right.set_product(right.numerator, left.denominator);
    return cross_left.compare(cross_right);
}

void reduce(positive_ratio& ratio)
{
    integer divisor;
    fmpz_gcd(divisor.native_handle(), ratio.numerator.native_handle(), ratio.denominator.native_handle());
    ratio.numerator.divide_exact(divisor);
    ratio.denominator.divide_exact(divisor);
}

positive_ratio calculate_lambda(integer::const_reference alpha, integer::const_reference beta,
                                integer::const_reference gamma)
{
    positive_ratio lambda1;
    positive_ratio lambda2;
    positive_ratio lambda3;

    lambda1.numerator.set_abs(gamma);
    lambda1.denominator.set_difference(alpha, gamma);

    lambda3.numerator = beta;
    lambda3.denominator.set_difference(beta, gamma);

    lambda2.numerator = lambda3.denominator;
    lambda2.denominator = alpha;
    lambda2.denominator -= gamma;
    lambda2.denominator -= gamma;
    lambda2.denominator += beta;

    positive_ratio result = compare(lambda2, lambda3) < 0 ? lambda2 : lambda3;
    if (compare(result, lambda1) < 0) result = lambda1;
    reduce(result);
    return result;
}

void divide_by_content(matrix_integer& gram)
{
    integer content;
    integer next;
    const size_t dimension = gram.rows();
    for (size_t i = 0; i < dimension; ++i) {
        timeout_checkpoint();
        for (size_t j = i; j < dimension; ++j) {
            fmpz_gcd(next.native_handle(), content.native_handle(), gram(i, j).native_handle());
            content.swap(next);
            if (content.is_one()) return;
        }
    }
    fmpz_mat_scalar_divexact_fmpz(gram.native_handle(), gram.native_handle(), content.native_handle());
}

std::pair<matrix_integer, matrix_integer> sponsel_split(const matrix_integer& gram, size_t first, size_t second,
                                                        const positive_ratio& lambda)
{
    integer complement;
    complement.set_difference(lambda.denominator, lambda.numerator);

    integer denominator_squared;
    denominator_squared.set_product(lambda.denominator, lambda.denominator);

    const size_t dimension = gram.rows();
    std::vector<integer> new_row(dimension);
    for (size_t k = 0; k < dimension; ++k) {
        new_row[k].set_product(lambda.numerator, gram(first, k));
        new_row[k].addmul(complement, gram(second, k));
        fmpz_mul(new_row[k].native_handle(), new_row[k].native_handle(), lambda.denominator.native_handle());
    }

    integer new_diagonal;
    integer coefficient;
    integer work;
    coefficient.set_product(lambda.numerator, lambda.numerator);
    new_diagonal.set_product(coefficient, gram(first, first));
    coefficient.set_product(lambda.numerator, complement);
    work.set_product(coefficient, gram(first, second));
    work.multiply(2);
    new_diagonal += work;
    coefficient.set_product(complement, complement);
    new_diagonal.addmul(coefficient, gram(second, second));

    matrix_integer first_child(gram);
    matrix_integer second_child(gram);
    fmpz_mat_scalar_mul_fmpz(first_child.native_handle(), first_child.native_handle(), denominator_squared.native_handle());
    fmpz_mat_scalar_mul_fmpz(second_child.native_handle(), second_child.native_handle(), denominator_squared.native_handle());

    for (size_t k = 0; k < dimension; ++k) {
        if (k != first) {
            first_child(first, k) = new_row[k];
            first_child(k, first) = new_row[k];
        }
        if (k != second) {
            second_child(second, k) = new_row[k];
            second_child(k, second) = new_row[k];
        }
    }
    first_child(first, first) = new_diagonal;
    second_child(second, second) = new_diagonal;
    divide_by_content(first_child);
    divide_by_content(second_child);
    return {std::move(first_child), std::move(second_child)};
}

/*
 * Coposit-created adaptive combination of Sponsel's H-enhanced Bundfuss split and Xu-Yao COPOMATRIX projection.
 *
 * A node chooses the first COPOMATRIX pivot with the minimum exact immediate-child count. It uses that pivot immediately when the
 * minimum is at most two; otherwise it applies Sponsel's certificate and split. After 1,000 consecutive same-order Sponsel splits on
 * a branch, it forces COPOMATRIX at the same minimum-child pivot and resets the streak in reduced children.
 */
class adaptive_sponsel_copomatrix_checker {
public:
    adaptive_sponsel_copomatrix_checker(size_t maximum_dimension, copositivity_mode mode)
        : h_factorization_(maximum_dimension)
        , mode_(mode)
        , progress_(progress::metric::adaptive, maximum_dimension)
    {
    }

    ~adaptive_sponsel_copomatrix_checker() { progress_.finish(); }

    bool check(const matrix_integer& matrix, size_t sponsel_streak = 0, long double weight = 1.0L, size_t depth = 0)
    {
        timeout_checkpoint();
        progress_.visit(matrix.rows(), depth);
        bool result;
        if (decide_small(matrix, result)) {
            if (result) progress_.resolved(weight);
            return result;
        }

        const size_t dimension = matrix.rows();
        for (size_t i = 0; i < dimension; ++i) {
            if (diagonal_fails(matrix(i, i))) return false;
        }

        progress_.adaptive_routing(sponsel_streak);
        const copomatrix_pivot pivot = minimum_child_copomatrix_pivot(matrix);
        const uint64_t pivot_children = capped_child_count(pivot.children);
        if (fmpz_cmp_ui(pivot.children.native_handle(), 2) <= 0 || sponsel_streak >= sponsel_streak_limit) {
            progress_.split();
            const bool forced = fmpz_cmp_ui(pivot.children.native_handle(), 2) > 0;
            progress_.adaptive_copomatrix(sponsel_streak, pivot.index, pivot_children, forced);
            return check_copomatrix(matrix, pivot, weight, depth);
        }
        progress_.split();
        progress_.adaptive_sponsel(sponsel_streak, pivot.index, pivot_children);
        return check_sponsel(matrix, sponsel_streak, weight, depth);
    }

#ifdef COPOSIT_ADAPTIVE_SPONSEL_COPOMATRIX_TESTING
    static size_t minimum_child_pivot_for_testing(const matrix_integer& matrix)
    {
        return minimum_child_copomatrix_pivot(matrix).index;
    }
#endif

private:
    bool decide_small(const matrix_integer& matrix, bool& result) const
    {
        if (matrix.rows() > 3) return false;
        result = matrix.rows() == 0 || small_copositivity::check(matrix, mode_);
        return true;
    }

    bool diagonal_fails(integer::const_reference diagonal) const noexcept
    {
        return diagonal.sign() < (mode_ == copositivity_mode::copositive ? 0 : 1);
    }

    struct copomatrix_pivot {
        size_t index;
        integer children;
    };

    static uint64_t capped_child_count(const integer& children) noexcept
    {
        if (fmpz_bits(children.native_handle()) > std::numeric_limits<uint64_t>::digits) {
            return std::numeric_limits<uint64_t>::max();
        }
        return static_cast<uint64_t>(fmpz_get_ui(children.native_handle()));
    }

    static copomatrix_pivot minimum_child_copomatrix_pivot(const matrix_integer& matrix)
    {
        const size_t dimension = matrix.rows();
        copomatrix_pivot best{dimension, integer()};
        for (size_t pivot = 0; pivot < dimension; ++pivot) {
            timeout_checkpoint();
            size_t positive = 0;
            size_t negative = 0;
            for (size_t index = 0; index < dimension; ++index) {
                if (index == pivot) continue;
                const int sign = matrix(pivot, index).sign();
                positive += sign > 0;
                negative += sign < 0;
            }

            integer children;
            if (matrix(pivot, pivot).is_zero() || negative == 0) {
                children.set_one();
            } else if (positive == 0) {
                fmpz_set_ui(children.native_handle(), 2);
            } else {
                fmpz_bin_uiui(children.native_handle(), static_cast<ulong>(positive + negative - 1), static_cast<ulong>(positive));
                fmpz_add_ui(children.native_handle(), children.native_handle(), 1);
            }

            if (best.index == dimension || children.compare(best.children) < 0) {
                best.index = pivot;
                best.children.swap(children);
            }
        }
        return best;
    }

    bool check_copomatrix(const matrix_integer& matrix, const copomatrix_pivot& pivot, long double weight, size_t depth)
    {
        const size_t dimension = matrix.rows();
        const size_t pivot_index = pivot.index;
        const double child_count = progress_.active() ? fmpz_get_d(pivot.children.native_handle()) : 0.0;
        const long double child_weight = child_count > 0.0 ? weight / child_count : 0.0L;
        const size_t child_dimension = dimension - 1;
        std::vector<size_t> remaining;
        std::vector<integer> p(child_dimension);
        std::vector<size_t> positive;
        std::vector<size_t> zero;
        std::vector<size_t> negative;
        remaining.reserve(child_dimension);
        positive.reserve(child_dimension);
        zero.reserve(child_dimension);
        negative.reserve(child_dimension);

        progress_.adaptive_stage(progress::adaptive_engine::copomatrix,
                                 progress::adaptive_phase::copomatrix_partition, 0, dimension);

        for (size_t index = 0; index < dimension; ++index) {
            if (index != pivot_index) remaining.push_back(index);
        }
        for (size_t index = 0; index < child_dimension; ++index) {
            p[index] = matrix(pivot_index, remaining[index]);
            if (p[index].sign() > 0) {
                positive.push_back(index);
            } else if (p[index].sign() < 0) {
                negative.push_back(index);
            } else {
                zero.push_back(index);
            }
        }

        progress_.adaptive_stage(progress::adaptive_engine::copomatrix,
                                 progress::adaptive_phase::principal_block, 0, child_dimension);
        matrix_integer block = make_principal_block(matrix, remaining);
        if (!check_projection(block, child_weight, depth + 1)) return false;
        if (matrix(pivot_index, pivot_index).is_zero()) return negative.empty();
        if (negative.empty()) return true;

        progress_.adaptive_stage(progress::adaptive_engine::copomatrix,
                                 progress::adaptive_phase::schur_block, 0, child_dimension);
        matrix_integer schur = make_schur_block(matrix, pivot_index, remaining, p);
        if (positive.empty()) return check_projection(schur, child_weight, depth + 1);

        std::vector<sparse_ray> rays;
        rays.reserve(child_dimension);
        for (const size_t index : zero) rays.push_back(coordinate_ray(index));
        return check_negative_staircase(schur, p, positive, negative, 0, 0, rays, child_weight, depth + 1);
    }

    bool check_projection(const matrix_integer& matrix, long double weight, size_t depth)
    {
        progress_.adaptive_copomatrix_child();
        const size_t dimension = matrix.rows();
        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            if (diagonal_fails(matrix(i, i))) return false;
        }
        for (size_t i = 0; i < dimension; ++i) {
            for (size_t j = i + 1; j < dimension; ++j) {
                if (matrix(i, j).sign() < 0) return check(matrix, 0, weight, depth);
            }
        }
        progress_.resolved(weight);
        return true;
    }

    bool check_negative_staircase(const matrix_integer& schur, const std::vector<integer>& p,
                                  const std::vector<size_t>& positive, const std::vector<size_t>& negative,
                                  size_t positive_begin, size_t negative_begin, std::vector<sparse_ray>& rays,
                                  long double weight, size_t depth)
    {
        timeout_checkpoint();
        progress_.adaptive_copomatrix_staircase();
        const size_t saved_size = rays.size();

        if (positive_begin == positive.size()) {
            for (size_t j = negative_begin; j < negative.size(); ++j) rays.push_back(coordinate_ray(negative[j]));
            progress_.adaptive_stage(progress::adaptive_engine::copomatrix,
                                     progress::adaptive_phase::transform, 0, rays.size());
            const bool result = check_projection(transform(schur, rays), weight, depth);
            rays.resize(saved_size);
            return result;
        }

        if (negative_begin + 1 == negative.size()) {
            rays.push_back(coordinate_ray(negative[negative_begin]));
            for (size_t i = positive_begin; i < positive.size(); ++i) {
                rays.push_back(pair_ray(p, positive[i], negative[negative_begin]));
            }
            progress_.adaptive_stage(progress::adaptive_engine::copomatrix,
                                     progress::adaptive_phase::transform, 0, rays.size());
            const bool result = check_projection(transform(schur, rays), weight, depth);
            rays.resize(saved_size);
            return result;
        }

        rays.push_back(pair_ray(p, positive[positive_begin], negative[negative_begin]));
        if (!check_negative_staircase(schur, p, positive, negative, positive_begin + 1, negative_begin, rays, weight, depth)) {
            rays.resize(saved_size);
            return false;
        }
        const bool result = check_negative_staircase(
            schur, p, positive, negative, positive_begin, negative_begin + 1, rays, weight, depth);
        rays.resize(saved_size);
        return result;
    }

    bool check_sponsel(const matrix_integer& matrix, size_t sponsel_streak, long double weight, size_t depth)
    {
        const size_t dimension = matrix.rows();
        progress_.adaptive_stage(progress::adaptive_engine::sponsel, progress::adaptive_phase::edge_scan, 0, dimension);
        size_t split_i = dimension;
        size_t split_j = dimension;
        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            for (size_t j = i + 1; j < dimension; ++j) {
                const integer::const_reference entry = matrix(i, j);
                if (entry.sign() < 0 && (split_i == dimension || entry.compare(matrix(split_i, split_j)) < 0)) {
                    split_i = i;
                    split_j = j;
                }
            }
        }

        if (split_i == dimension) {
            progress_.resolved(weight);
            return true;
        }

        integer diagonal_product;
        integer edge_squared;
        diagonal_product.set_product(matrix(split_i, split_i), matrix(split_j, split_j));
        edge_squared.set_product(matrix(split_i, split_j), matrix(split_i, split_j));
        const int edge_comparison = edge_squared.compare(diagonal_product);
        if (edge_comparison > 0 || (edge_comparison == 0 && mode_ == copositivity_mode::strictly_copositive)) return false;

        if (passes_h_certificate(matrix)) {
            progress_.resolved(weight);
            return true;
        }

        progress_.adaptive_stage(progress::adaptive_engine::sponsel, progress::adaptive_phase::split_build);
        const positive_ratio lambda = calculate_lambda(matrix(split_i, split_i), matrix(split_j, split_j), matrix(split_i, split_j));
        auto children = sponsel_split(matrix, split_i, split_j, lambda);
        progress_.adaptive_sponsel_split();
        long double first_weight = 0.0L;
        if (progress_.active()) {
            slong numerator_exponent;
            slong denominator_exponent;
            const long double numerator = lambda.numerator.to_dbl_2exp(numerator_exponent);
            const long double denominator = lambda.denominator.to_dbl_2exp(denominator_exponent);
            first_weight = weight * std::ldexp(numerator / denominator,
                                               static_cast<int>(numerator_exponent - denominator_exponent));
        }
        const size_t child_streak = sponsel_streak + 1;
        if (!check(children.first, child_streak, first_weight, depth + 1)) return false;
        return check(children.second, child_streak, progress_.active() ? weight - first_weight : 0.0L, depth + 1);
    }

    bool passes_h_certificate(const matrix_integer& gram)
    {
        const size_t dimension = gram.rows();
        progress_.adaptive_stage(progress::adaptive_engine::sponsel, progress::adaptive_phase::h_build, 0, dimension);
        matrix_integer stripped(dimension, dimension);
        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            stripped(i, i) = gram(i, i);
            for (size_t j = i + 1; j < dimension; ++j) {
                if (gram(i, j).sign() < 0) {
                    stripped(i, j) = gram(i, j);
                    stripped(j, i) = gram(i, j);
                }
            }
        }

        progress_.adaptive_stage(progress::adaptive_engine::sponsel,
                                 progress::adaptive_phase::h_factorization, 0, dimension);
        h_factorization_.factorize_inplace(stripped, false, &progress_);
        return mode_ == copositivity_mode::copositive
            ? h_factorization_.is_positive_semidefinite()
            : h_factorization_.is_positive_definite();
    }

    static matrix_integer make_principal_block(const matrix_integer& matrix, const std::vector<size_t>& remaining)
    {
        const size_t dimension = remaining.size();
        matrix_integer block(dimension, dimension);
        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            for (size_t j = i; j < dimension; ++j) {
                block(i, j) = matrix(remaining[i], remaining[j]);
                if (i != j) block(j, i) = block(i, j);
            }
        }
        return block;
    }

    static matrix_integer make_schur_block(const matrix_integer& matrix, size_t pivot_index,
                                           const std::vector<size_t>& remaining, const std::vector<integer>& p)
    {
        const size_t dimension = remaining.size();
        const integer::const_reference pivot = matrix(pivot_index, pivot_index);
        matrix_integer schur(dimension, dimension);
        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            for (size_t j = i; j < dimension; ++j) {
                schur(i, j).set_product(pivot, matrix(remaining[i], remaining[j]));
                schur(i, j).submul(p[i], p[j]);
                if (i != j) schur(j, i) = schur(i, j);
            }
        }
        return schur;
    }

    static sparse_ray coordinate_ray(size_t index)
    {
        sparse_ray ray;
        ray.indices[0] = index;
        ray.coefficients[0].set_one();
        return ray;
    }

    static sparse_ray pair_ray(const std::vector<integer>& p, size_t positive, size_t negative)
    {
        sparse_ray ray;
        ray.count = 2;
        ray.indices[0] = positive;
        ray.indices[1] = negative;
        ray.coefficients[0].set_abs(p[negative]);
        ray.coefficients[1] = p[positive];

        integer divisor;
        fmpz_gcd(divisor.native_handle(), ray.coefficients[0].native_handle(), ray.coefficients[1].native_handle());
        ray.coefficients[0].divide_exact(divisor);
        ray.coefficients[1].divide_exact(divisor);
        return ray;
    }

    static matrix_integer transform(const matrix_integer& matrix, const std::vector<sparse_ray>& rays)
    {
        const size_t dimension = rays.size();
        matrix_integer result(dimension, dimension);
        integer coefficient;
        for (size_t row = 0; row < dimension; ++row) {
            timeout_checkpoint();
            for (size_t column = row; column < dimension; ++column) {
                for (size_t left = 0; left < rays[row].count; ++left) {
                    for (size_t right = 0; right < rays[column].count; ++right) {
                        coefficient.set_product(rays[row].coefficients[left], rays[column].coefficients[right]);
                        result(row, column).addmul(
                            coefficient, matrix(rays[row].indices[left], rays[column].indices[right]));
                    }
                }
                if (row != column) result(column, row) = result(row, column);
            }
        }
        return result;
    }

    fraction_free_ldlt_factorization h_factorization_;
    const copositivity_mode mode_;
    progress::tracker progress_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    adaptive_sponsel_copomatrix_checker checker(dimension, mode);
    return checker.check(matrix);
}

#ifdef COPOSIT_ADAPTIVE_SPONSEL_COPOMATRIX_TESTING
namespace adaptive_sponsel_copomatrix_testing {

size_t streak_limit() noexcept
{
    return sponsel_streak_limit;
}

bool solve_with_streak(const matrix_integer& matrix, size_t sponsel_streak, copositivity_mode mode)
{
    adaptive_sponsel_copomatrix_checker checker(matrix.rows(), mode);
    return checker.check(matrix, sponsel_streak);
}

size_t minimum_child_pivot(const matrix_integer& matrix)
{
    return adaptive_sponsel_copomatrix_checker::minimum_child_pivot_for_testing(matrix);
}

} // namespace adaptive_sponsel_copomatrix_testing
#endif

} // namespace coposit::model
