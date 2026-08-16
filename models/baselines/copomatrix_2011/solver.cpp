#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/timeout.hpp>

#include "../source_diagnostics.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace coposit::model {

namespace {

struct sparse_ray {
    std::array<size_t, 2> indices{};
    std::array<integer, 2> coefficients{};
    size_t count = 1;
};

/*
 * Exact non-strict implementation and strict adaptation of Xu and Yao's 2011 COPOMATRIX projection algorithm.
 *
 * Each node checks the principal child first, then triangulates only the negative pivot half-simplex for the Schur form. Xu and
 * Yao's normalized midpoints are represented by positively scaled primitive integer rays. Their unspecified pending-polytope
 * choice is fixed as Danninger's deterministic staircase order: delete the first positive label before the first negative label.
 */
class copomatrix_checker {
public:
    copomatrix_checker(copositivity_mode mode, size_t dimension)
        : mode_(mode), diagnostics_(diagnostics::metric::proof, dimension)
    {
    }

    ~copomatrix_checker() { diagnostics_.finish(); }

    bool check(const matrix_integer& matrix, long double weight = 1.0L, size_t depth = 0)
    {
        timeout_checkpoint();
        const size_t dimension = matrix.rows();
        diagnostics_.visit(dimension, depth, depth + 1);
        COPOSIT_SOURCE_DIAGNOSTICS("diagonal-scan", dimension);
        for (size_t i = 0; i < dimension; ++i) {
            if (diagonal_fails(matrix(i, i))) return false;
        }
        if (dimension == 1) {
            diagnostics_.resolved(weight);
            return true;
        }

        const size_t child_dimension = dimension - 1;
        const integer::const_reference pivot = matrix(0, 0);
        std::vector<integer> p(child_dimension);
        std::vector<size_t> positive;
        std::vector<size_t> zero;
        std::vector<size_t> negative;
        positive.reserve(child_dimension);
        zero.reserve(child_dimension);
        negative.reserve(child_dimension);

        for (size_t i = 0; i < child_dimension; ++i) {
            p[i] = matrix(0, i + 1);
            if (p[i].sign() > 0) {
                positive.push_back(i);
            } else if (p[i].sign() < 0) {
                negative.push_back(i);
            } else {
                zero.push_back(i);
            }
        }

        diagnostics_.split();
        long double child_weight = 0.0L;
        if (diagnostics_.active()) {
            integer child_count;
            if (pivot.is_zero() || negative.empty()) {
                child_count.set_one();
            } else if (positive.empty()) {
                fmpz_set_ui(child_count.native_handle(), 2);
            } else {
                fmpz_bin_uiui(child_count.native_handle(), static_cast<ulong>(positive.size() + negative.size() - 1),
                              static_cast<ulong>(positive.size()));
                fmpz_add_ui(child_count.native_handle(), child_count.native_handle(), 1);
            }
            child_weight = weight / fmpz_get_d(child_count.native_handle());
        }

        matrix_integer block = make_principal_block(matrix);
        COPOSIT_SOURCE_DIAGNOSTICS("principal", child_dimension);
        if (!check_projection(block, child_weight, depth + 1)) return false;
        if (pivot.is_zero()) return negative.empty();
        if (negative.empty()) return true;

        matrix_integer schur = make_schur_block(matrix, block, p, pivot);
        COPOSIT_SOURCE_DIAGNOSTICS("schur", child_dimension);
        if (positive.empty()) return check_projection(schur, child_weight, depth + 1);

        std::vector<sparse_ray> rays;
        rays.reserve(child_dimension);
        for (const size_t index : zero) rays.push_back(coordinate_ray(index));
        return check_negative_staircase(schur, p, positive, negative, 0, 0, rays, child_weight, depth + 1);
    }

private:
    bool diagonal_fails(integer::const_reference diagonal) const noexcept
    {
        return diagonal.sign() < (mode_ == copositivity_mode::copositive ? 0 : 1);
    }

    bool check_projection(const matrix_integer& matrix, long double weight, size_t depth)
    {
        const size_t dimension = matrix.rows();
        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            if (diagonal_fails(matrix(i, i))) return false;
        }
        for (size_t i = 0; i < dimension; ++i) {
            for (size_t j = i + 1; j < dimension; ++j) {
                if (matrix(i, j).sign() < 0) return check(matrix, weight, depth);
            }
        }
        diagnostics_.resolved(weight);
        return true;
    }

    bool check_negative_staircase(const matrix_integer& schur, const std::vector<integer>& p,
                                  const std::vector<size_t>& positive, const std::vector<size_t>& negative,
                                  size_t positive_begin, size_t negative_begin, std::vector<sparse_ray>& rays,
                                  long double weight, size_t depth)
    {
        timeout_checkpoint();
        const size_t saved_size = rays.size();

        if (positive_begin == positive.size()) {
            for (size_t j = negative_begin; j < negative.size(); ++j) rays.push_back(coordinate_ray(negative[j]));
            const bool result = check_projection(transform(schur, rays), weight, depth);
            rays.resize(saved_size);
            return result;
        }

        if (negative_begin + 1 == negative.size()) {
            rays.push_back(coordinate_ray(negative[negative_begin]));
            for (size_t i = positive_begin; i < positive.size(); ++i) {
                rays.push_back(pair_ray(p, positive[i], negative[negative_begin]));
            }
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

    static matrix_integer make_principal_block(const matrix_integer& matrix)
    {
        const size_t dimension = matrix.rows() - 1;
        matrix_integer block(dimension, dimension);
        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            for (size_t j = i; j < dimension; ++j) {
                block(i, j) = matrix(i + 1, j + 1);
                if (i != j) block(j, i) = block(i, j);
            }
        }
        return block;
    }

    static matrix_integer make_schur_block(const matrix_integer& matrix, const matrix_integer& block,
                                           const std::vector<integer>& p, integer::const_reference pivot)
    {
        const size_t dimension = block.rows();
        matrix_integer schur(dimension, dimension);
        for (size_t i = 0; i < dimension; ++i) {
            timeout_checkpoint();
            for (size_t j = i; j < dimension; ++j) {
                schur(i, j).set_product(pivot, block(i, j));
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

    const copositivity_mode mode_;
    diagnostics::tracker diagnostics_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    copomatrix_checker checker(mode, dimension);
    return checker.check(matrix);
}

} // namespace coposit::model
