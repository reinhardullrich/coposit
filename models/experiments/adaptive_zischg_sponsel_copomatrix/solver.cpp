#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/timeout.hpp>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

constexpr size_t sponsel_streak_limit = 10000;

#ifdef COPOSIT_ADAPTIVE_ZISCHG_SPONSEL_COPOMATRIX_TESTING
size_t component_decomposition_count = 0;
#endif

bool is_strictly_copositive_1x1(integer::const_reference b11) noexcept
{
    return b11.sign() > 0;
}

bool is_strictly_copositive_2x2(integer::const_reference b11, integer::const_reference b12,
                                integer::const_reference b22) noexcept
{
    if (b11.sign() <= 0 || b22.sign() <= 0) return false;
    if (b12.sign() >= 0) return true;

    integer determinant;
    determinant.set_product(b11, b22);
    determinant.submul(b12, b12);
    return determinant.sign() > 0;
}

bool is_strictly_copositive_3x3(integer::const_reference b11, integer::const_reference b12,
                                integer::const_reference b13, integer::const_reference b22,
                                integer::const_reference b23, integer::const_reference b33) noexcept
{
    if (b11.sign() <= 0 || b22.sign() <= 0 || b33.sign() <= 0) return false;

    integer work;
    if (b12.sign() < 0) {
        work.set_product(b11, b22);
        work.submul(b12, b12);
        if (work.sign() <= 0) return false;
    }
    if (b13.sign() < 0) {
        work.set_product(b11, b33);
        work.submul(b13, b13);
        if (work.sign() <= 0) return false;
    }
    if (b23.sign() < 0) {
        work.set_product(b22, b33);
        work.submul(b23, b23);
        if (work.sign() <= 0) return false;
    }

    integer determinant;
    determinant.set_product(b11, b22);
    fmpz_mul(determinant.native_handle(), determinant.native_handle(), b33.native_handle());
    work.set_product(b12, b13);
    fmpz_mul(work.native_handle(), work.native_handle(), b23.native_handle());
    work.multiply(2);
    determinant += work;
    work.set_product(b23, b23);
    determinant.submul(b11, work);
    work.set_product(b13, b13);
    determinant.submul(b22, work);
    work.set_product(b12, b12);
    determinant.submul(b33, work);

    if (determinant.sign() > 0) return true;

    work.set_product(b22, b33);
    work.submul(b23, b23);
    if (work.sign() <= 0) return true;
    work.set_product(b11, b33);
    work.submul(b13, b13);
    if (work.sign() <= 0) return true;
    work.set_product(b11, b22);
    work.submul(b12, b12);
    if (work.sign() <= 0) return true;
    work.set_product(b13, b23);
    work.submul(b12, b33);
    if (work.sign() <= 0) return true;
    work.set_product(b12, b23);
    work.submul(b13, b22);
    if (work.sign() <= 0) return true;
    work.set_product(b12, b13);
    work.submul(b11, b23);
    return work.sign() <= 0;
}

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

std::pair<matrix_integer, matrix_integer> sponsel_split(const matrix_integer& gram, size_t first, size_t second)
{
    const positive_ratio lambda = calculate_lambda(gram(first, first), gram(second, second), gram(first, second));
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
 * Coposit-created adaptive combination of Sponsel's H-enhanced Bundfuss split, Xu-Yao COPOMATRIX projection, and Zischg-Bomze
 * negative-graph decomposition after each projection.
 *
 * A node uses the first COPOMATRIX pivot producing at most two children. Otherwise it applies Sponsel's certificate and split. After
 * 10,000 consecutive same-order Sponsel splits on a branch, it forces COPOMATRIX at pivot zero and resets the streak in reduced children.
 */
class adaptive_zischg_sponsel_copomatrix_checker {
public:
    explicit adaptive_zischg_sponsel_copomatrix_checker(size_t maximum_dimension)
        : h_factorization_(maximum_dimension)
    {
    }

    bool check(const matrix_integer& matrix, size_t sponsel_streak = 0)
    {
        timeout_checkpoint();
        bool result;
        if (decide_small(matrix, result)) return result;

        const size_t dimension = matrix.rows();
        for (size_t i = 0; i < dimension; ++i) {
            if (matrix(i, i).sign() <= 0) return false;
        }

        const size_t pivot = first_narrow_copomatrix_pivot(matrix);
        if (pivot != dimension) return check_copomatrix(matrix, pivot);
        if (sponsel_streak >= sponsel_streak_limit) return check_copomatrix(matrix, 0);
        return check_sponsel(matrix, sponsel_streak);
    }

private:
    static bool decide_small(const matrix_integer& matrix, bool& result)
    {
        switch (matrix.rows()) {
            case 0:
                result = true;
                return true;
            case 1:
                result = is_strictly_copositive_1x1(matrix(0, 0));
                return true;
            case 2:
                result = is_strictly_copositive_2x2(matrix(0, 0), matrix(0, 1), matrix(1, 1));
                return true;
            case 3:
                result = is_strictly_copositive_3x3(
                    matrix(0, 0), matrix(0, 1), matrix(0, 2), matrix(1, 1), matrix(1, 2), matrix(2, 2));
                return true;
            default:
                return false;
        }
    }

    static size_t first_narrow_copomatrix_pivot(const matrix_integer& matrix)
    {
        const size_t dimension = matrix.rows();
        for (size_t pivot = 0; pivot < dimension; ++pivot) {
            timeout_checkpoint();
            size_t positive = 0;
            size_t negative = 0;
            for (size_t index = 0; index < dimension; ++index) {
                if (index == pivot) continue;
                const int sign = matrix(pivot, index).sign();
                positive += sign > 0;
                negative += sign < 0;
                if (positive != 0 && negative > 1) break;
            }
            if (positive == 0 || negative <= 1) return pivot;
        }
        return dimension;
    }

    bool check_copomatrix(const matrix_integer& matrix, size_t pivot_index)
    {
        const size_t dimension = matrix.rows();
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

        matrix_integer block = make_principal_block(matrix, remaining);
        if (!check_projection(block)) return false;
        if (negative.empty()) return true;

        matrix_integer schur = make_schur_block(matrix, pivot_index, remaining, p);
        if (positive.empty()) return check_projection(schur);

        std::vector<sparse_ray> rays;
        rays.reserve(child_dimension);
        for (const size_t index : zero) rays.push_back(coordinate_ray(index));
        return check_negative_staircase(schur, p, positive, negative, 0, 0, rays);
    }

    bool check_projection(const matrix_integer& matrix)
    {
        const size_t dimension = matrix.rows();
        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            if (matrix(i, i).sign() <= 0) return false;
        }
        bool has_negative_entry = false;
        for (size_t i = 0; i < dimension && !has_negative_entry; ++i) {
            for (size_t j = i + 1; j < dimension; ++j) {
                if (matrix(i, j).sign() < 0) {
                    has_negative_entry = true;
                    break;
                }
            }
        }
        if (!has_negative_entry) return true;

        const std::vector<std::vector<size_t>> components = negative_components(matrix);
        if (components.size() == 1) return check(matrix, 0);

#ifdef COPOSIT_ADAPTIVE_ZISCHG_SPONSEL_COPOMATRIX_TESTING
        ++component_decomposition_count;
#endif
        for (const std::vector<size_t>& component : components) {
            if (component.size() > 1 && !check(make_principal_block(matrix, component), 0)) return false;
        }
        return true;
    }

    static std::vector<std::vector<size_t>> negative_components(const matrix_integer& matrix)
    {
        const size_t dimension = matrix.rows();
        std::vector<unsigned char> visited(dimension);
        std::vector<std::vector<size_t>> components;
        std::vector<size_t> pending;
        pending.reserve(dimension);

        for (size_t start = 0; start < dimension; ++start) {
            timeout_checkpoint();
            if (visited[start]) continue;

            components.emplace_back();
            visited[start] = 1;
            pending.push_back(start);
            while (!pending.empty()) {
                timeout_checkpoint();
                const size_t vertex = pending.back();
                pending.pop_back();
                components.back().push_back(vertex);
                for (size_t candidate = 0; candidate < dimension; ++candidate) {
                    if (!visited[candidate] && matrix(vertex, candidate).sign() < 0) {
                        visited[candidate] = 1;
                        pending.push_back(candidate);
                    }
                }
            }
        }
        return components;
    }

    bool check_negative_staircase(const matrix_integer& schur, const std::vector<integer>& p,
                                  const std::vector<size_t>& positive, const std::vector<size_t>& negative,
                                  size_t positive_begin, size_t negative_begin, std::vector<sparse_ray>& rays)
    {
        timeout_checkpoint();
        const size_t saved_size = rays.size();

        if (positive_begin == positive.size()) {
            for (size_t j = negative_begin; j < negative.size(); ++j) rays.push_back(coordinate_ray(negative[j]));
            const bool result = check_projection(transform(schur, rays));
            rays.resize(saved_size);
            return result;
        }

        if (negative_begin + 1 == negative.size()) {
            rays.push_back(coordinate_ray(negative[negative_begin]));
            for (size_t i = positive_begin; i < positive.size(); ++i) {
                rays.push_back(pair_ray(p, positive[i], negative[negative_begin]));
            }
            const bool result = check_projection(transform(schur, rays));
            rays.resize(saved_size);
            return result;
        }

        rays.push_back(pair_ray(p, positive[positive_begin], negative[negative_begin]));
        if (!check_negative_staircase(schur, p, positive, negative, positive_begin + 1, negative_begin, rays)) {
            rays.resize(saved_size);
            return false;
        }
        const bool result = check_negative_staircase(schur, p, positive, negative, positive_begin, negative_begin + 1, rays);
        rays.resize(saved_size);
        return result;
    }

    bool check_sponsel(const matrix_integer& matrix, size_t sponsel_streak)
    {
        const size_t dimension = matrix.rows();
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

        if (split_i == dimension) return true;

        integer diagonal_product;
        integer edge_squared;
        diagonal_product.set_product(matrix(split_i, split_i), matrix(split_j, split_j));
        edge_squared.set_product(matrix(split_i, split_j), matrix(split_i, split_j));
        if (edge_squared.compare(diagonal_product) >= 0) return false;

        if (passes_strict_h_certificate(matrix)) return true;

        auto children = sponsel_split(matrix, split_i, split_j);
        const size_t child_streak = sponsel_streak + 1;
        if (!check(children.first, child_streak)) return false;
        return check(children.second, child_streak);
    }

    bool passes_strict_h_certificate(const matrix_integer& gram)
    {
        const size_t dimension = gram.rows();
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

        h_factorization_.factorize_inplace(stripped);
        return h_factorization_.is_positive_definite();
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
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    require_strict_mode(mode);
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
#ifdef COPOSIT_ADAPTIVE_ZISCHG_SPONSEL_COPOMATRIX_TESTING
    component_decomposition_count = 0;
#endif
    adaptive_zischg_sponsel_copomatrix_checker checker(dimension);
    return checker.check(matrix);
}

#ifdef COPOSIT_ADAPTIVE_ZISCHG_SPONSEL_COPOMATRIX_TESTING
namespace adaptive_zischg_sponsel_copomatrix_testing {

size_t streak_limit() noexcept
{
    return sponsel_streak_limit;
}

bool solve_with_streak(const matrix_integer& matrix, size_t sponsel_streak)
{
    component_decomposition_count = 0;
    adaptive_zischg_sponsel_copomatrix_checker checker(matrix.rows());
    return checker.check(matrix, sponsel_streak);
}

size_t decomposition_count() noexcept
{
    return component_decomposition_count;
}

} // namespace adaptive_zischg_sponsel_copomatrix_testing
#endif

} // namespace coposit::model
