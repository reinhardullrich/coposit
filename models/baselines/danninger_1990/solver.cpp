#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/timeout.hpp>

#include "../source_trace.hpp"

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

struct staircase_frame {
    size_t row;
    size_t column;
    unsigned char next_branch = 0;
};

class active_node_scope {
public:
    explicit active_node_scope(size_t& open_nodes) : open_nodes_(open_nodes)
    {
        enforce_open_node_limit(open_nodes_ + 1);
        ++open_nodes_;
    }

    ~active_node_scope() { --open_nodes_; }

    active_node_scope(const active_node_scope&) = delete;
    active_node_scope& operator=(const active_node_scope&) = delete;

private:
    size_t& open_nodes_;
};

/*
 * Exact ordinary- and strict-copositivity reconstruction of Danninger's 1990 dimension-reducing recursion.
 *
 * For A = [[a, p^T], [p, B]] with a > 0, eliminate the fixed first coordinate. On p^T y >= 0 the child form is B;
 * on p^T y <= 0 it is the division-free Schur form aB - pp^T. Mixed-sign half-orthants use the same lazy staircase
 * triangulation and child order as the retained FracESSA reconstruction. Every recursive child has order one less.
 */
class danninger_checker {
public:
    explicit danninger_checker(copositivity_mode mode) : mode_(mode) {}
    explicit danninger_checker(copositivity_classification& classification)
        : mode_(copositivity_mode::copositive), classification_(&classification)
    {
    }

    bool check(const matrix_integer& matrix)
    {
        const active_node_scope current_node(open_nodes_);
        timeout_checkpoint();
        const size_t dimension = matrix.rows();
        switch (dimension) {
            case 0:
                return true;
            case 1:
            case 2:
            case 3: {
                if (classification_ != nullptr) {
                    const copositivity_classification current = small_copositivity::classify(matrix);
                    classification_->is_strictly_copositive &= current.is_strictly_copositive;
                    return current.is_copositive;
                }
                return small_copositivity::check(matrix, mode_);
            }
            default:
                break;
        }

        for (size_t i = 0; i < dimension; ++i) {
            const int sign = matrix(i, i).sign();
            if (classification_ != nullptr) {
                if (sign < 0) return false;
                if (sign == 0) classification_->is_strictly_copositive = false;
            } else if (sign < (mode_ == copositivity_mode::copositive ? 0 : 1)) {
                return false;
            }
        }

        const size_t child_dimension = dimension - 1;
        const integer::const_reference pivot = matrix(0, 0);
        std::vector<integer> p(child_dimension);
        matrix_integer block(child_dimension, child_dimension);
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

        for (size_t i = 0; i < child_dimension; ++i) {
            timeout_checkpoint();
            for (size_t j = i; j < child_dimension; ++j) {
                block(i, j) = matrix(i + 1, j + 1);
                if (i != j) block(j, i) = block(i, j);
            }
        }

        if (pivot.is_zero()) {
            for (const integer& entry : p) {
                if (entry.sign() < 0) return false;
            }
            return check(block);
        }

        // With no mixed signs, the complete-orthant child includes the zero-coordinate face of the other half-cone.
        if (negative.empty()) {
            COPOSIT_SOURCE_TRACE("block", dimension);
            return check(block);
        }
        if (positive.empty()) {
            COPOSIT_SOURCE_TRACE("schur", dimension);
            return check(make_schur(block, pivot, p));
        }

        const std::vector<sparse_ray> pair_rays = make_pair_rays(p, positive, negative);

        std::vector<sparse_ray> rays;
        rays.reserve(child_dimension);
        for (const size_t index : zero) rays.push_back(coordinate_ray(index));
        rays.push_back(plus_ray(positive, negative, pair_rays, 0, 0));
        COPOSIT_SOURCE_TRACE("plus", positive.size(), negative.size());
        if (!check_plus_paths(block, positive, negative, pair_rays, 0, 0, rays)) return false;

        const matrix_integer schur = make_schur(block, pivot, p);
        rays.clear();
        for (const size_t index : zero) rays.push_back(coordinate_ray(index));
        rays.push_back(minus_ray(negative, pair_rays, 0, 0));
        COPOSIT_SOURCE_TRACE("minus", positive.size(), negative.size());
        return check_minus_paths(schur, positive, negative, pair_rays, 0, 0, rays);
    }

private:
    bool check_plus_paths(const matrix_integer& matrix, const std::vector<size_t>& positive,
                          const std::vector<size_t>& negative, const std::vector<sparse_ray>& pair_rays,
                          size_t row, size_t column, std::vector<sparse_ray>& rays)
    {
        std::vector<staircase_frame> path;
        path.reserve(64); // Initial capacity only; the shared open-node limit is the actual bound.
        push_path_frame(path, row, column);

        while (!path.empty()) {
            timeout_checkpoint();
            staircase_frame& current = path.back();
            if (current.row + 1 == positive.size() && current.column == negative.size()) {
                enforce_open_node_limit(open_nodes_ + 1);
                if (!check(transform(matrix, rays))) {
                    discard_path(path);
                    return false;
                }
                pop_path_frame(path, rays);
                continue;
            }

            if (current.next_branch == 0) {
                current.next_branch = 1;
                if (current.row + 1 < positive.size()) {
                    rays.push_back(plus_ray(positive, negative, pair_rays, current.row + 1, current.column));
                    push_path_frame(path, current.row + 1, current.column);
                    continue;
                }
            }
            if (current.next_branch == 1) {
                current.next_branch = 2;
                if (current.column < negative.size()) {
                    rays.push_back(plus_ray(positive, negative, pair_rays, current.row, current.column + 1));
                    push_path_frame(path, current.row, current.column + 1);
                    continue;
                }
            }
            pop_path_frame(path, rays);
        }
        return true;
    }

    bool check_minus_paths(const matrix_integer& matrix, const std::vector<size_t>& positive,
                           const std::vector<size_t>& negative, const std::vector<sparse_ray>& pair_rays,
                           size_t row, size_t column, std::vector<sparse_ray>& rays)
    {
        std::vector<staircase_frame> path;
        path.reserve(64); // Initial capacity only; the shared open-node limit is the actual bound.
        push_path_frame(path, row, column);

        while (!path.empty()) {
            timeout_checkpoint();
            staircase_frame& current = path.back();
            if (current.row == positive.size() && current.column + 1 == negative.size()) {
                enforce_open_node_limit(open_nodes_ + 1);
                if (!check(transform(matrix, rays))) {
                    discard_path(path);
                    return false;
                }
                pop_path_frame(path, rays);
                continue;
            }

            if (current.next_branch == 0) {
                current.next_branch = 1;
                if (current.row < positive.size()) {
                    rays.push_back(minus_ray(negative, pair_rays, current.row + 1, current.column));
                    push_path_frame(path, current.row + 1, current.column);
                    continue;
                }
            }
            if (current.next_branch == 1) {
                current.next_branch = 2;
                if (current.column + 1 < negative.size()) {
                    rays.push_back(minus_ray(negative, pair_rays, current.row, current.column + 1));
                    push_path_frame(path, current.row, current.column + 1);
                    continue;
                }
            }
            pop_path_frame(path, rays);
        }
        return true;
    }

    void push_path_frame(std::vector<staircase_frame>& path, size_t row, size_t column)
    {
        enforce_open_node_limit(open_nodes_ + 1);
        path.push_back({row, column});
        ++open_nodes_;
    }

    void pop_path_frame(std::vector<staircase_frame>& path, std::vector<sparse_ray>& rays)
    {
        path.pop_back();
        --open_nodes_;
        if (!path.empty()) rays.pop_back();
    }

    void discard_path(std::vector<staircase_frame>& path)
    {
        open_nodes_ -= path.size();
        path.clear();
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

    static std::vector<sparse_ray> make_pair_rays(const std::vector<integer>& p, const std::vector<size_t>& positive,
                                                  const std::vector<size_t>& negative)
    {
        std::vector<sparse_ray> rays;
        rays.reserve(positive.size() * negative.size());
        for (const size_t positive_index : positive) {
            timeout_checkpoint();
            for (const size_t negative_index : negative) rays.push_back(pair_ray(p, positive_index, negative_index));
        }
        return rays;
    }

    static sparse_ray plus_ray(const std::vector<size_t>& positive, const std::vector<size_t>& negative,
                               const std::vector<sparse_ray>& pair_rays, size_t row, size_t column)
    {
        if (column == 0) return coordinate_ray(positive[row]);
        return pair_rays[row * negative.size() + column - 1];
    }

    static sparse_ray minus_ray(const std::vector<size_t>& negative, const std::vector<sparse_ray>& pair_rays,
                                size_t row, size_t column)
    {
        if (row == 0) return coordinate_ray(negative[column]);
        return pair_rays[(row - 1) * negative.size() + column];
    }

    static matrix_integer make_schur(const matrix_integer& block, integer::const_reference pivot, const std::vector<integer>& p)
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
    copositivity_classification* classification_ = nullptr;
    size_t open_nodes_ = 0;
};

void validate_input(const matrix_integer& matrix)
{
    const size_t dimension = matrix.rows();
    if (dimension == 0 || matrix.cols() != dimension) throw std::invalid_argument("matrix must be nonempty and square");

    for (size_t i = 0; i < dimension; ++i) {
        for (size_t j = i + 1; j < dimension; ++j) {
            if (matrix(i, j).compare(matrix(j, i)) != 0) throw std::invalid_argument("matrix must be symmetric");
        }
    }
}

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    validate_input(matrix);

    danninger_checker checker(mode);
    return checker.check(matrix);
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    validate_input(matrix);

    copositivity_classification result{true, true};
    danninger_checker checker(result);
    if (!checker.check(matrix)) result = {false, false};
    return result;
}

} // namespace coposit::model
