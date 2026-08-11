#include <coposit/model.hpp>
#include <coposit/timeout.hpp>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace coposit::model {

namespace {

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

/*
 * Coposit-created adaptive combination of Danninger's dimension reduction and Dutour Sikirić's cone split.
 *
 * A node first uses the first pivot whose Danninger triangulation has at most two children. If no such pivot exists, it performs
 * one maximum-ratio Dutour split. Every child repeats the same choice from its first row. Algorithm code is intentionally local to
 * this model so its routing, reductions, and cone updates can evolve independently.
 */
class adaptive_dutour_danninger_checker {
public:
    bool check(const matrix_integer& matrix)
    {
        timeout_checkpoint();
        bool result;
        if (decide_small(matrix, result)) return result;

        const size_t dimension = matrix.rows();
        for (size_t i = 0; i < dimension; ++i) {
            if (matrix(i, i).sign() <= 0) return false;
        }

        const size_t pivot = first_narrow_danninger_pivot(matrix);
        if (pivot != dimension) return check_danninger(matrix, pivot);
        return check_dutour(matrix);
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

    static size_t first_narrow_danninger_pivot(const matrix_integer& matrix)
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
                if (positive != 0 && negative != 0 && positive + negative > 2) break;
            }
            if (positive == 0 || negative == 0 || positive + negative == 2) return pivot;
        }
        return dimension;
    }

    bool check_danninger(const matrix_integer& matrix, size_t pivot_index)
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

        if (negative.empty()) return check(make_principal_block(matrix, remaining));
        if (positive.empty()) return check(make_schur_block(matrix, pivot_index, remaining, p));

        matrix_integer block = make_principal_block(matrix, remaining);
        matrix_integer schur = make_schur_block(matrix, pivot_index, remaining, p);
        const sparse_ray mixed_ray = pair_ray(p, positive.front(), negative.front());

        std::vector<sparse_ray> rays;
        rays.reserve(child_dimension);
        for (const size_t index : zero) rays.push_back(coordinate_ray(index));
        rays.push_back(coordinate_ray(positive.front()));
        rays.push_back(mixed_ray);
        if (!check(transform(block, rays))) return false;

        rays.clear();
        for (const size_t index : zero) rays.push_back(coordinate_ray(index));
        rays.push_back(coordinate_ray(negative.front()));
        rays.push_back(mixed_ray);
        return check(transform(schur, rays));
    }

    bool check_dutour(const matrix_integer& matrix)
    {
        const size_t dimension = matrix.rows();
        size_t split_i = dimension;
        size_t split_j = dimension;
        integer best_numerator;
        integer best_denominator;
        integer numerator;
        integer denominator;
        integer left;
        integer right;

        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            for (size_t j = i + 1; j < dimension; ++j) {
                const integer::const_reference entry = matrix(i, j);
                if (entry.sign() >= 0) continue;

                numerator.set_product(entry, entry);
                denominator.set_product(matrix(i, i), matrix(j, j));
                if (numerator.compare(denominator) >= 0) return false;

                bool take_pair = split_i == dimension;
                if (!take_pair) {
                    left.set_product(numerator, best_denominator);
                    right.set_product(best_numerator, denominator);
                    take_pair = left.compare(right) > 0;
                }
                if (take_pair) {
                    split_i = i;
                    split_j = j;
                    best_numerator = numerator;
                    best_denominator = denominator;
                }
            }
        }

        if (split_i == dimension) return true;

        matrix_integer first_child(matrix);
        matrix_integer second_child(matrix);
        replace_generator_with_sum(first_child, split_i, split_j);
        replace_generator_with_sum(second_child, split_j, split_i);
        if (!check(first_child)) return false;
        return check(second_child);
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

    static void replace_generator_with_sum(matrix_integer& matrix, size_t replaced, size_t other)
    {
        integer new_diagonal(matrix(replaced, replaced));
        new_diagonal += matrix(replaced, other);
        new_diagonal += matrix(replaced, other);
        new_diagonal += matrix(other, other);

        const size_t dimension = matrix.rows();
        for (size_t k = 0; k < dimension; ++k) {
            timeout_checkpoint();
            if (k == replaced) continue;
            integer sum(matrix(replaced, k));
            sum += matrix(other, k);
            matrix(replaced, k) = sum;
            matrix(k, replaced) = sum;
        }
        matrix(replaced, replaced) = new_diagonal;
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

};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    require_strict_mode(mode);
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension == 0 || matrix.cols() != dimension) throw std::invalid_argument("matrix must be nonempty and square");

    for (size_t i = 0; i < dimension; ++i) {
        timeout_checkpoint();
        for (size_t j = i + 1; j < dimension; ++j) {
            if (matrix(i, j).compare(matrix(j, i)) != 0) throw std::invalid_argument("matrix must be symmetric");
        }
    }

    adaptive_dutour_danninger_checker checker;
    return checker.check(matrix);
}

} // namespace coposit::model
