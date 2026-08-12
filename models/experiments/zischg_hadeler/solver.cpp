#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cassert>
#include <cstddef>
#include <stdexcept>
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

class hadeler_checker {
public:
    explicit hadeler_checker(const matrix_integer& matrix)
        : factorization_(matrix.rows())
        , negative_graph_(matrix)
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
                if (subset_dimension > 3 && !negative_graph_.induced_is_connected(current_support)) {
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
                    ++level_two_skips;
#endif
                    continue;
                }
                if (!check_subset(matrix, indices)) return false;
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

    bool check_subset(const matrix_integer& matrix, const std::vector<size_t>& indices)
    {
        const size_t dimension = indices.size();
        if (dimension <= 3) return small_copositivity::check_principal(matrix, indices.data(), dimension);

        principal_.resize(dimension, dimension);
        copy_principal(matrix, indices, principal_);

        const bool nonsingular = factorization_.factorize_inplace(principal_) != 0;
        const int determinant_sign = factorization_.determinant().sign();
        if (determinant_sign > 0) return true;

        if (nonsingular) {
            solution_.resize(dimension, 1);
            const integer minus_one(-1);
            for (size_t row = 0; row < dimension; ++row) solution_(row, 0) = minus_one;

            integer denominator;
            factorization_.solve_inplace(solution_, denominator, principal_);
            assert(denominator.sign() > 0);
            for (size_t row = 0; row < dimension; ++row) {
                if (solution_(row, 0).sign() <= 0) return true;
            }
            return false;
        }

        if (dimension - factorization_.rank() != 1) return true;

        solution_.resize(dimension, 1);
        factorization_.one_nullspace_vector(solution_, principal_);
        const int basis_sign = solution_(0, 0).sign();
        if (basis_sign == 0) return true;
        for (size_t row = 1; row < dimension; ++row) {
            if (solution_(row, 0).sign() != basis_sign) return true;
        }
        return false;
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column) principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

    fraction_free_ldlt_factorization factorization_;
    negative_graph negative_graph_;
    matrix_integer principal_;
    matrix_integer solution_;
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
    if (dimension <= 3) return small_copositivity::check(matrix);
#ifdef COPOSIT_ZISCHG_LEVEL_TWO_TESTING
    level_two_skips = 0;
#endif
    return hadeler_checker(matrix).check(matrix);
}

} // namespace coposit::model
