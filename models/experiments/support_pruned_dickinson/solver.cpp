#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/small_copositivity.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

class support_generator {
public:
    explicit support_generator(size_t dimension)
        : dimension_(dimension)
        , forbidden_by_lowest_(dimension)
        , partial_support_(dimension)
    {
    }

    template<class Callback>
    void generate(Callback&& callback)
    {
        for (target_cardinality_ = 1; target_cardinality_ <= dimension_; ++target_cardinality_) {
            activate_pending();
            emitted_ = false;
            if (!generate_from(dimension_, target_cardinality_, callback)) return;
            if (!emitted_) return;
        }
    }

    void add_forbidden(const support& forbidden)
    {
        pending_forbidden_.push_back(forbidden);
    }

private:
    void activate_pending()
    {
        for (support& forbidden : pending_forbidden_) {
            forbidden_by_lowest_[forbidden.lowest_index()].push_back(std::move(forbidden));
        }
        pending_forbidden_.clear();
    }

    bool completes_forbidden(size_t new_lowest_bit) const noexcept
    {
        for (const support& forbidden : forbidden_by_lowest_[new_lowest_bit]) {
            if (forbidden.is_subset_of(partial_support_)) return true;
        }
        return false;
    }

    template<class Callback>
    bool generate_from(size_t bits_remaining, size_t needed, Callback& callback)
    {
        timeout_checkpoint();
        if (needed == 0) {
            emitted_ = true;
            return callback(partial_support_, target_cardinality_);
        }
        if (needed > bits_remaining) return true;

        const size_t bit = bits_remaining - 1;
        if (needed < bits_remaining && !generate_from(bit, needed, callback)) return false;

        partial_support_.set(bit);
        const bool keep_going = completes_forbidden(bit) || generate_from(bit, needed - 1, callback);
        partial_support_.reset(bit);
        return keep_going;
    }

    size_t dimension_;
    std::vector<std::vector<support>> forbidden_by_lowest_;
    std::vector<support> pending_forbidden_;
    support partial_support_;
    size_t target_cardinality_ = 0;
    bool emitted_ = false;
};

struct certificate_signature {
    explicit certificate_signature(size_t dimension)
        : nonzero_support(dimension)
        , product_nonnegative(dimension)
    {
    }

    support nonzero_support;
    support product_nonnegative;
};

class dickinson_checker {
public:
    explicit dickinson_checker(size_t dimension)
        : factorization_(dimension)
        , product_(dimension)
        , certificates_by_lowest_(dimension)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        support_generator generator(matrix.rows());
        bool strictly_copositive = true;
        generator.generate([&](const support& current_support, size_t subset_dimension) {
            current_support.copy_indices_to(indices_);
            if (subset_dimension <= 3
                && !small_copositivity::check_principal(matrix, indices_.data(), subset_dimension)) {
                strictly_copositive = false;
                return false;
            }
            if (!is_covered(current_support, indices_) && !process_subset(matrix, indices_, generator)) {
                strictly_copositive = false;
                return false;
            }
            return true;
        });
        return strictly_copositive;
    }

private:
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

    bool process_subset(const matrix_integer& matrix, const std::vector<size_t>& indices, support_generator& generator)
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

        add_certificate(matrix, indices, solution_, generator);
        return true;
    }

    void add_certificate(
        const matrix_integer& matrix, const std::vector<size_t>& indices, const matrix_integer& solution, support_generator& generator)
    {
        certificate_signature certificate(matrix.rows());
        for (size_t local = 0; local < indices.size(); ++local) {
            if (!solution(local, 0).is_zero()) certificate.nonzero_support.set(indices[local]);
        }

        bool covers_every_superset = true;
        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices.size(); ++local) {
                product_[row].addmul(matrix(row, indices[local]), solution(local, 0));
            }
            if (product_[row].sign() >= 0) {
                certificate.product_nonnegative.set(row);
            } else {
                covers_every_superset = false;
            }
        }

        if (covers_every_superset) generator.add_forbidden(certificate.nonzero_support);
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
    std::vector<std::vector<certificate_signature>> certificates_by_lowest_;
    std::vector<size_t> indices_;
};

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    require_strict_mode(mode);
    timeout_checkpoint();
    const size_t dimension = matrix.rows();
    if (dimension <= 3) return small_copositivity::check(matrix);

    return dickinson_checker(dimension).check(matrix);
}

} // namespace coposit::model
