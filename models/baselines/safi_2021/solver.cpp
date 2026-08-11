#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>
#include <coposit/timeout.hpp>

#include "../source_trace.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

enum class inspection { certified, rejected, split };

struct simplex {
    matrix_integer vertices;
    std::vector<integer> denominators;
    matrix_integer gram;

    explicit simplex(size_t dimension) : vertices(dimension, dimension), denominators(dimension), gram(dimension, dimension) {}
};

struct sparse_vertex {
    std::array<size_t, 2> indices{};
    std::array<integer, 2> coefficients{};
    integer denominator;
    size_t count = 1;
};

struct slice_frame {
    explicit slice_frame(simplex&& value) : parent(std::move(value)) {}

    simplex parent;
    std::vector<size_t> unchanged;
    std::vector<size_t> cut;
    std::vector<sparse_vertex> intersections;
    size_t next_child = 0;
};

/*
 * Exact ordinary implementation and strict adaptation of Safi, Nabavi, and Caron's 2021 SNC simplex slicing.
 *
 * Each vertex is z_i / d_i with positive d_i. The stored Gram entry is z_i^T A z_j, whose sign equals the sign of the
 * corresponding rational Gram entry. Slicing and the center-radius certificate therefore use fraction-free integer arithmetic.
 */
class snc_checker {
public:
    snc_checker(const matrix_integer& matrix, copositivity_mode mode)
        : dimension_(matrix.rows()), matrix_norm_one_(elementwise_norm_one(matrix)), mode_(mode)
    {
    }

    bool check(const matrix_integer& matrix) const
    {
        simplex current(dimension_);
        current.vertices.set_identity(dimension_);
        for (integer& denominator : current.denominators) denominator.set_one();
        current.gram = matrix;

        std::vector<slice_frame> path;
        path.reserve(64); // Initial capacity only; this is not a dimension limit.
        size_t open_nodes = 1;

        while (true) {
            timeout_checkpoint();
            --open_nodes;

            size_t slicing_vertex = dimension_;
            switch (inspect(current, slicing_vertex)) {
                case inspection::rejected:
                    COPOSIT_SOURCE_TRACE("rejected");
                    return false;
                case inspection::certified:
                    COPOSIT_SOURCE_TRACE("certified");
                    while (!path.empty() && path.back().next_child == path.back().cut.size()) path.pop_back();
                    if (path.empty()) return true;
                    current = make_next_child(path.back());
                    break;
                case inspection::split:
                    COPOSIT_SOURCE_TRACE("split", slicing_vertex);
                    path.push_back(begin_slice(std::move(current), slicing_vertex));
                    enforce_open_node_limit(open_nodes + path.back().cut.size());
                    open_nodes += path.back().cut.size();
                    current = make_next_child(path.back());
                    break;
            }
        }
    }

private:
    static integer elementwise_norm_one(const matrix_integer& matrix)
    {
        integer result;
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column < matrix.cols(); ++column) {
                if (matrix(row, column).sign() < 0) {
                    result -= matrix(row, column);
                } else {
                    result += matrix(row, column);
                }
            }
        }
        return result;
    }

    inspection inspect(const simplex& current, size_t& slicing_vertex) const
    {
        for (size_t i = 0; i < dimension_; ++i) {
            if (current.gram(i, i).sign() < (mode_ == copositivity_mode::copositive ? 0 : 1)) return inspection::rejected;
        }

        slicing_vertex = dimension_;
        for (size_t i = 0; i < dimension_; ++i) {
            timeout_checkpoint();
            for (size_t j = i + 1; j < dimension_; ++j) {
                if (current.gram(i, j).sign() >= 0) continue;
                if (mode_ == copositivity_mode::copositive
                    && (current.gram(i, i).is_zero() || current.gram(j, j).is_zero())) return inspection::rejected;
                if (slicing_vertex == dimension_) slicing_vertex = i;
            }
        }
        if (slicing_vertex == dimension_) return inspection::certified;
        return inspect_center_and_radius(current);
    }

    inspection inspect_center_and_radius(const simplex& current) const
    {
        // With L = lcm(d_i), the simplex center is y/(nL), where y = sum_i (L/d_i) z_i.
        integer common_denominator(1);
        for (const integer& denominator : current.denominators) {
            fmpz_lcm(common_denominator.native_handle(), common_denominator.native_handle(), denominator.native_handle());
        }

        std::vector<integer> factors(dimension_);
        for (size_t i = 0; i < dimension_; ++i) {
            fmpz_divexact(factors[i].native_handle(), common_denominator.native_handle(), current.denominators[i].native_handle());
        }

        // n^2 L^2 times the center value is y^T A y = sum_ij factor_i factor_j G_ij.
        integer center_numerator;
        integer coefficient;
        for (size_t i = 0; i < dimension_; ++i) {
            timeout_checkpoint();
            coefficient.set_product(factors[i], factors[i]);
            center_numerator.addmul(coefficient, current.gram(i, i));
            for (size_t j = i + 1; j < dimension_; ++j) {
                coefficient.set_product(factors[i], factors[j]);
                coefficient.multiply(2);
                center_numerator.addmul(coefficient, current.gram(i, j));
            }
        }
        if (center_numerator.sign() < (mode_ == copositivity_mode::copositive ? 0 : 1)) return inspection::rejected;

        std::vector<integer> center_coordinates(dimension_);
        for (size_t coordinate = 0; coordinate < dimension_; ++coordinate) {
            timeout_checkpoint();
            for (size_t vertex = 0; vertex < dimension_; ++vertex) {
                center_coordinates[coordinate].addmul(factors[vertex], current.vertices(coordinate, vertex));
            }
        }

        // The L1 radius is R/(nL), where R is the largest distance numerator.
        integer radius;
        integer distance;
        integer difference;
        integer scaled_coordinate;
        for (size_t vertex = 0; vertex < dimension_; ++vertex) {
            timeout_checkpoint();
            distance.set_zero();
            for (size_t coordinate = 0; coordinate < dimension_; ++coordinate) {
                scaled_coordinate.set_product(factors[vertex], current.vertices(coordinate, vertex));
                scaled_coordinate.multiply(dimension_);
                difference.set_difference(scaled_coordinate, center_coordinates[coordinate]);
                difference.set_abs(difference);
                distance += difference;
            }
            if (radius.compare(distance) < 0) radius = distance;
        }

        // ||A||_1(delta^2 + 2delta) < center_value, cleared of the common positive denominators.
        integer radius_polynomial;
        radius_polynomial.set_product(radius, radius);
        integer linear_term;
        linear_term.set_product(radius, common_denominator);
        linear_term.multiply(2);
        linear_term.multiply(dimension_);
        radius_polynomial += linear_term;
        integer bound;
        bound.set_product(matrix_norm_one_, radius_polynomial);
        const int comparison = bound.compare(center_numerator);
        return comparison < 0 || (comparison == 0 && mode_ == copositivity_mode::copositive)
            ? inspection::certified
            : inspection::split;
    }

    slice_frame begin_slice(simplex&& parent, size_t slicing_vertex) const
    {
        slice_frame frame(std::move(parent));
        frame.unchanged.reserve(dimension_);
        frame.cut.reserve(dimension_);

        for (size_t j = 0; j < dimension_; ++j) {
            if (j == slicing_vertex) continue;
            if (frame.parent.gram(j, slicing_vertex).sign() >= 0) {
                frame.unchanged.push_back(j);
            } else {
                frame.cut.push_back(j);
            }
        }
        if (frame.cut.empty()) throw std::logic_error("Safi 2021 split selected no negative edge");

        frame.intersections.reserve(frame.cut.size());
        for (const size_t j : frame.cut) {
            frame.intersections.push_back(intersection_vertex(frame.parent, slicing_vertex, j));
        }
        return frame;
    }

    simplex make_next_child(slice_frame& frame) const
    {
        timeout_checkpoint();
        const size_t child_index = frame.next_child++;
        std::vector<sparse_vertex> columns;
        columns.reserve(dimension_);
        for (const size_t j : frame.unchanged) columns.push_back(original_vertex(frame.parent, j));
        for (size_t k = 0; k <= child_index; ++k) columns.push_back(original_vertex(frame.parent, frame.cut[k]));
        for (size_t k = child_index; k < frame.cut.size(); ++k) columns.push_back(frame.intersections[k]);
        if (columns.size() != dimension_) throw std::logic_error("Safi 2021 child has the wrong number of vertices");

        simplex child(dimension_);
        build_child_vertices(frame.parent, columns, child);
        build_child_gram(frame.parent.gram, columns, child.gram);
        return child;
    }

    static sparse_vertex original_vertex(const simplex& parent, size_t index)
    {
        sparse_vertex result;
        result.indices[0] = index;
        result.coefficients[0].set_one();
        result.denominator = parent.denominators[index];
        return result;
    }

    static sparse_vertex intersection_vertex(const simplex& parent, size_t slicing_vertex, size_t other_vertex)
    {
        const integer::const_reference diagonal = parent.gram(slicing_vertex, slicing_vertex);
        const integer::const_reference cross = parent.gram(other_vertex, slicing_vertex);

        sparse_vertex result;
        result.count = 2;
        result.indices[0] = slicing_vertex;
        result.indices[1] = other_vertex;

        integer negative_cross;
        fmpz_neg(negative_cross.native_handle(), cross.native_handle());
        integer common_factor;
        fmpz_gcd(common_factor.native_handle(), diagonal.native_handle(), negative_cross.native_handle());
        fmpz_divexact(result.coefficients[0].native_handle(), negative_cross.native_handle(), common_factor.native_handle());
        fmpz_divexact(result.coefficients[1].native_handle(), diagonal.native_handle(), common_factor.native_handle());

        result.denominator.set_product(diagonal, parent.denominators[other_vertex]);
        result.denominator.addmul(negative_cross, parent.denominators[slicing_vertex]);
        result.denominator.divide_exact(common_factor);
        return result;
    }

    void build_child_vertices(const simplex& parent, const std::vector<sparse_vertex>& columns, simplex& child) const
    {
        for (size_t column = 0; column < dimension_; ++column) {
            timeout_checkpoint();
            child.denominators[column] = columns[column].denominator;
            for (size_t coordinate = 0; coordinate < dimension_; ++coordinate) {
                for (size_t component = 0; component < columns[column].count; ++component) {
                    child.vertices(coordinate, column).addmul(
                        parent.vertices(coordinate, columns[column].indices[component]), columns[column].coefficients[component]);
                }
            }
        }
    }

    void build_child_gram(const matrix_integer& parent, const std::vector<sparse_vertex>& columns, matrix_integer& child) const
    {
        integer coefficient;
        for (size_t row = 0; row < dimension_; ++row) {
            timeout_checkpoint();
            for (size_t column = row; column < dimension_; ++column) {
                for (size_t left = 0; left < columns[row].count; ++left) {
                    for (size_t right = 0; right < columns[column].count; ++right) {
                        coefficient.set_product(columns[row].coefficients[left], columns[column].coefficients[right]);
                        child(row, column).addmul(
                            parent(columns[row].indices[left], columns[column].indices[right]), coefficient);
                    }
                }
                if (row != column) child(column, row) = child(row, column);
            }
        }
    }

    const size_t dimension_;
    const integer matrix_norm_one_;
    const copositivity_mode mode_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension == 0 || matrix.cols() != dimension) throw std::invalid_argument("matrix must be nonempty and square");

    for (size_t i = 0; i < dimension; ++i) {
        for (size_t j = i + 1; j < dimension; ++j) {
            if (matrix(i, j).compare(matrix(j, i)) != 0) throw std::invalid_argument("matrix must be symmetric");
        }
    }

    return snc_checker(matrix, mode).check(matrix);
}

} // namespace coposit::model
