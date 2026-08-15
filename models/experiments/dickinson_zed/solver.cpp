#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/progress.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_trace.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
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

class maximal_z_blocks {
public:
    explicit maximal_z_blocks(const matrix_integer& matrix)
    {
        adjacency_.reserve(matrix.rows());
        for (size_t index = 0; index < matrix.rows(); ++index) adjacency_.emplace_back(matrix.rows());

        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t column = row + 1; column < matrix.rows(); ++column) {
                if (matrix(row, column).sign() > 0) continue;
                adjacency_[row].set(column);
                adjacency_[column].set(row);
            }
        }
    }

    template <class Visitor>
    bool visit(Visitor&& visitor) const
    {
        const size_t dimension = adjacency_.size();
        support block(dimension);
        support candidates(dimension);
        support excluded(dimension);
        candidates.set_all();
        return search(block, std::move(candidates), std::move(excluded), visitor);
    }

private:
    template <class Visitor>
    bool search(support& block, support candidates, support excluded, Visitor& visitor) const
    {
        timeout_checkpoint();
        if (candidates.empty() && excluded.empty()) {
            std::vector<size_t> indices;
            block.copy_indices_to(indices);
            return indices.size() < 2 || visitor(block, indices);
        }

        const size_t pivot = !candidates.empty() ? candidates.lowest_index() : excluded.lowest_index();
        support extensions = candidates;
        extensions.remove(adjacency_[pivot]);
        std::vector<size_t> vertices;
        extensions.copy_indices_to(vertices);

        for (const size_t vertex : vertices) {
            support child_candidates = candidates;
            support child_excluded = excluded;
            child_candidates.intersect_with(adjacency_[vertex]);
            child_excluded.intersect_with(adjacency_[vertex]);

            block.set(vertex);
            if (!search(block, std::move(child_candidates), std::move(child_excluded), visitor)) return false;
            block.reset(vertex);
            candidates.reset(vertex);
            excluded.set(vertex);
        }
        return true;
    }

    std::vector<support> adjacency_;
};

class dickinson_checker {
public:
    explicit dickinson_checker(size_t dimension)
        : factorization_(dimension)
        , product_(dimension)
        , certificates_by_lowest_(dimension)
        , progress_(progress::metric::support, dimension)
    {
    }

    bool check(const matrix_integer& matrix)
    {
        if (!maximal_z_blocks(matrix).visit([&](const support& block, const std::vector<size_t>& indices) {
                return process_z_block(matrix, block, indices);
            })) {
            progress_.finish();
            return false;
        }

        const size_t matrix_dimension = matrix.rows();
        for (size_t subset_dimension = 1; subset_dimension <= matrix_dimension; ++subset_dimension) {
            progress_.stage(subset_dimension);
            std::vector<size_t> indices(subset_dimension);
            support current_support(matrix_dimension);
            for (size_t i = 0; i < subset_dimension; ++i) {
                indices[i] = i;
                current_support.set(i);
            }

            do {
                timeout_checkpoint();
                progress_.visit_support();
                const bool covered = is_covered(current_support, indices);
                COPOSIT_DICKINSON_ZED_TRACE(covered ? "covered" : "process", subset_dimension);
                if (covered) {
                    progress_.covered_support();
                } else {
                    progress_.secondary();
                    if (!process_subset(matrix, indices)) {
                        progress_.finish();
                        return false;
                    }
                }
            } while (advance_numeric_mask_order(indices, current_support, matrix_dimension));
        }

        progress_.finish();
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
        for (const support& block : z_downsets_) {
            timeout_checkpoint();
            if (current_support.is_subset_of(block)) return true;
        }

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

    bool process_z_block(const matrix_integer& matrix, const support& block, const std::vector<size_t>& indices)
    {
        std::vector<bool> reached(indices.size());
        std::vector<size_t> queue;
        std::vector<size_t> component;
        for (size_t start = 0; start < indices.size(); ++start) {
            if (reached[start]) continue;
            queue.assign(1, start);
            reached[start] = true;
            component.clear();
            for (size_t next = 0; next < queue.size(); ++next) {
                const size_t local = queue[next];
                component.push_back(indices[local]);
                for (size_t candidate = 0; candidate < indices.size(); ++candidate) {
                    if (!reached[candidate] && matrix(indices[local], indices[candidate]).sign() < 0) {
                        reached[candidate] = true;
                        queue.push_back(candidate);
                    }
                }
            }

            principal_.resize(component.size(), component.size());
            copy_principal(matrix, component, principal_);
            factorization_.factorize_inplace(principal_);
            COPOSIT_DICKINSON_ZED_TRACE("z-component", component.size());
            if (!factorization_.is_positive_definite()) {
                COPOSIT_DICKINSON_ZED_TRACE("z-reject", indices.size());
                return false;
            }
        }

        COPOSIT_DICKINSON_ZED_TRACE("z-covered", indices.size());
        z_downsets_.push_back(block);
        return true;
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
            for (size_t row = 0; row < dimension; ++row) {
                has_positive_entry |= solution_(row, 0).sign() > 0;
            }
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
        progress_.certificate();
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
            for (size_t column = 0; column <= row; ++column) {
                principal(row, column) = matrix(indices[row], indices[column]);
            }
        }
    }

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    std::vector<integer> product_;
    std::vector<support> z_downsets_;
    std::vector<std::vector<certificate_signature>> certificates_by_lowest_;
    progress::tracker progress_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    require_strict_mode(mode);
    timeout_checkpoint();
    return dickinson_checker(matrix.rows()).check(matrix);
}

#ifdef COPOSIT_DICKINSON_ZED_TESTING
std::vector<uint64_t> maximal_z_block_masks(const matrix_integer& matrix)
{
    assert(matrix.rows() <= 64);
    std::vector<uint64_t> result;
    maximal_z_blocks(matrix).visit([&](const support&, const std::vector<size_t>& indices) {
        uint64_t mask = 0;
        for (const size_t index : indices) mask |= uint64_t{1} << index;
        result.push_back(mask);
        return true;
    });
    std::sort(result.begin(), result.end());
    return result;
}
#endif

} // namespace coposit::model
