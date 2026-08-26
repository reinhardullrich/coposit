#include <coposit/diagnostics.hpp>
#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_diagnostics.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

    uint64_t saturated_binomial(size_t n, size_t k) noexcept
    {
        if (k > n)
            return 0;
        k = std::min(k, n - k);
        uint64_t result = 1;
        for (size_t i = 1; i <= k; ++i) {
            size_t numerator = n - k + i;
            size_t denominator = i;
            const size_t numerator_divisor = std::gcd(numerator, denominator);
            numerator /= numerator_divisor;
            denominator /= numerator_divisor;
            const uint64_t result_divisor = std::gcd(result, static_cast<uint64_t>(denominator));
            result /= result_divisor;
            denominator /= static_cast<size_t>(result_divisor);
            assert(denominator == 1);
            if (numerator != 0 && result > std::numeric_limits<uint64_t>::max() / numerator)
                return std::numeric_limits<uint64_t>::max();
            result *= numerator;
        }
        return result;
    }

    class support_generator {
    public:
        explicit support_generator(size_t dimension, diagnostics::tracker* diagnostics = nullptr)
            : support_context_(dimension)
            , dimension_(dimension)
            , forbidden_by_lowest_(dimension)
            , partial_support_(support_context_.make())
            , diagnostics_(diagnostics)
        {
        }

        template <class Callback> bool generate(Callback&& callback)
        {
            for (target_cardinality_ = 1; target_cardinality_ <= dimension_; ++target_cardinality_) {
                activate_pending();
                emitted_ = false;
                if (diagnostics_ != nullptr)
                    diagnostics_->support_cardinality(target_cardinality_);
                if (!generate_from(dimension_, target_cardinality_, callback))
                    return false;
                if (!emitted_) {
                    if (diagnostics_ != nullptr)
                        for (size_t remaining = target_cardinality_ + 1; remaining <= dimension_; ++remaining)
                            diagnostics_->skip_supports(saturated_binomial(dimension_, remaining));
                    return true;
                }
            }
            return true;
        }

        void add_forbidden(support lower)
        {
            assert(!support_context_.empty(lower));
            pending_forbidden_.push_back(std::move(lower));
        }

#ifdef COPOSIT_AFFINE_COMPANION_DICKINSON_TESTING
        uint64_t mask_for_testing(const support& value) const noexcept { return support_context_.small_bits(value); }
        void add_forbidden_copy_for_testing(const support& lower) { add_forbidden(support_context_.clone(lower)); }
#endif

    private:
        void activate_pending()
        {
            for (support& forbidden : pending_forbidden_)
                forbidden_by_lowest_[support_context_.first(forbidden)].push_back(std::move(forbidden));
            pending_forbidden_.clear();
        }

        bool completes_forbidden(size_t new_lowest_bit) const noexcept
        {
            for (const support& forbidden : forbidden_by_lowest_[new_lowest_bit])
                if (support_context_.is_subset_of(forbidden, partial_support_))
                    return true;
            return false;
        }

        template <class Callback> bool generate_from(size_t bits_remaining, size_t needed, Callback& callback)
        {
            timeout_checkpoint();
            if (needed == 0) {
                emitted_ = true;
                if (diagnostics_ != nullptr)
                    diagnostics_->visit_support();
                return callback(partial_support_, target_cardinality_);
            }
            if (needed > bits_remaining)
                return true;

            const size_t bit = bits_remaining - 1;
            if (needed < bits_remaining && !generate_from(bit, needed, callback))
                return false;

            support_context_.set(partial_support_, bit);
            bool keep_going = true;
            if (completes_forbidden(bit)) {
                if (diagnostics_ != nullptr)
                    diagnostics_->skip_supports(saturated_binomial(bit, needed - 1));
            } else {
                keep_going = generate_from(bit, needed - 1, callback);
            }
            support_context_.reset(partial_support_, bit);
            return keep_going;
        }

        support_context support_context_;
        size_t dimension_;
        std::vector<std::vector<support>> forbidden_by_lowest_;
        std::vector<support> pending_forbidden_;
        support partial_support_;
        diagnostics::tracker* diagnostics_;
        size_t target_cardinality_ = 0;
        bool emitted_ = false;
    };

    struct signed_ratio {
        integer numerator;
        integer denominator;
    };

    size_t exact_nullspace(const matrix_integer& matrix, matrix_integer& basis)
    {
        const size_t columns = matrix.cols();
        basis.resize(columns, columns);
        if (matrix.rows() == 0) {
            basis.set_identity(columns);
            return columns;
        }
        timeout_checkpoint();
        const size_t nullity = static_cast<size_t>(fmpz_mat_nullspace(basis.native_handle(), matrix.native_handle()));
        timeout_checkpoint();
        return nullity;
    }

    void canonicalize_ray(matrix_integer& ray)
    {
        integer content;
        integer next;
        for (size_t row = 0; row < ray.rows(); ++row) {
            fmpz_gcd(next.native_handle(), content.native_handle(), ray(row, 0).native_handle());
            content.swap(next);
        }
        assert(!content.is_zero());
        if (!content.is_one())
            for (size_t row = 0; row < ray.rows(); ++row)
                ray(row, 0).divide_exact(content);

        for (size_t row = 0; row < ray.rows(); ++row) {
            if (ray(row, 0).is_zero())
                continue;
            if (ray(row, 0).sign() < 0)
                ray.negate();
            return;
        }
        assert(false);
    }

    std::string ray_key(const matrix_integer& ray)
    {
        std::string key;
        for (size_t row = 0; row < ray.rows(); ++row) {
            if (row != 0)
                key.push_back(',');
            key += integer(ray(row, 0)).to_string();
        }
        return key;
    }

    bool advance_combination(std::vector<size_t>& combination, size_t universe_size) noexcept
    {
        for (size_t position = combination.size(); position-- > 0;) {
            const size_t maximum = universe_size - combination.size() + position;
            if (combination[position] == maximum)
                continue;
            ++combination[position];
            for (size_t next = position + 1; next < combination.size(); ++next)
                combination[next] = combination[next - 1] + 1;
            return true;
        }
        return false;
    }

    class dickinson_checker {
    public:
        dickinson_checker(size_t dimension, copositivity_mode mode)
            : support_context_(dimension)
            , factorization_(dimension)
            , mode_(mode)
            , diagnostics_(diagnostics::metric::support, dimension)
            , supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr)
        {
            indices_.reserve(dimension);
            outside_indices_.reserve(dimension);
        }

        dickinson_checker(size_t dimension, copositivity_classification& classification)
            : support_context_(dimension)
            , factorization_(dimension)
            , mode_(copositivity_mode::copositive)
            , classification_(&classification)
            , diagnostics_(diagnostics::metric::support, dimension)
            , supports_(dimension, diagnostics_.active() ? &diagnostics_ : nullptr)
        {
            indices_.reserve(dimension);
            outside_indices_.reserve(dimension);
        }

        bool check(const matrix_integer& matrix)
        {
            const bool result = supports_.generate([&](const support& current, size_t cardinality) {
                timeout_checkpoint();
                support_context_.extract_set_indices(current, indices_);
                COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("process", cardinality);
                return process_subset(matrix);
            });
            diagnostics_.finish();
            return result;
        }

#ifdef COPOSIT_AFFINE_COMPANION_DICKINSON_TESTING
        bool search_kernel_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices)
        {
            indices_ = indices;
            principal_.resize(indices_.size(), indices_.size());
            copy_principal(matrix, indices_, principal_);
            if (factorization_.factorize_inplace(principal_) != 0 || indices_.size() - factorization_.rank() <= 1)
                return false;
            prepare_root_kernel(matrix);
            return search_root_kernel(matrix);
        }

        bool search_affine_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices)
        {
            indices_ = indices;
            principal_.resize(indices_.size(), indices_.size());
            copy_principal(matrix, indices_, principal_);
            if (factorization_.factorize_inplace(principal_) != 0)
                return false;
            prepare_root_kernel(matrix);
            return search_affine_companion(matrix, indices_.size() - factorization_.rank());
        }
#endif

    private:
        bool process_subset(const matrix_integer& matrix)
        {
            diagnostics_.secondary();
            const size_t dimension = indices_.size();
            principal_.resize(dimension, dimension);
            solution_.resize(dimension, 1);
            copy_principal(matrix, indices_, principal_);

            const bool singular = factorization_.factorize_inplace(principal_) == 0;
            if (!singular) {
                for (size_t row = 0; row < dimension; ++row)
                    solution_(row, 0).set_one();

                integer denominator;
                factorization_.solve_inplace(solution_, denominator, principal_);
                assert(denominator.sign() > 0);
            } else {
                const size_t nullity = dimension - factorization_.rank();
                prepare_root_kernel(matrix);
                if (!search_affine_companion(matrix, nullity))
                    return false;

                for (size_t row = 0; row < dimension; ++row)
                    solution_(row, 0) = root_basis_(row, 0);

                bool has_positive_entry = false;
                for (size_t row = 0; row < dimension; ++row)
                    has_positive_entry |= solution_(row, 0).sign() > 0;
                if (!has_positive_entry)
                    solution_.negate();
            }

            bool all_nonpositive = true;
            bool all_nonnegative = singular;
            for (size_t row = 0; row < dimension; ++row) {
                all_nonpositive &= solution_(row, 0).sign() <= 0;
                all_nonnegative &= solution_(row, 0).sign() >= 0;
            }
            if (all_nonpositive)
                return false;
            if (all_nonnegative && !record_nonnegative_zero())
                return false;

            add_ceiling_certificate(matrix, solution_);
            if (singular && dimension - factorization_.rank() > 1 && !search_root_kernel(matrix))
                return false;
            return true;
        }

        void prepare_root_kernel(const matrix_integer& matrix)
        {
            const size_t nullity = indices_.size() - factorization_.rank();
            root_basis_.resize(indices_.size(), nullity);
            factorization_.nullspace_basis(root_basis_, principal_);
            build_outside_indices(matrix.rows());
        }

        void build_outside_indices(size_t matrix_order)
        {
            outside_indices_.clear();
            size_t local = 0;
            for (size_t row = 0; row < matrix_order; ++row) {
                if (local < indices_.size() && indices_[local] == row)
                    ++local;
                else
                    outside_indices_.push_back(row);
            }
        }

        bool search_affine_companion(const matrix_integer& matrix, size_t nullity)
        {
            for (size_t column = 0; column < nullity; ++column) {
                affine_sum_.set_zero();
                for (size_t row = 0; row < root_basis_.rows(); ++row)
                    affine_sum_ += root_basis_(row, column);
                if (!affine_sum_.is_zero()) {
                    COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("affine-inconsistent", nullity);
                    return true;
                }
            }

            affine_particular_.resize(indices_.size(), 1);
            for (size_t row = 0; row < indices_.size(); ++row)
                affine_particular_(row, 0).set_one();
            integer denominator;
            const bool consistent = factorization_.solve_consistent_inplace(affine_particular_, denominator, principal_);
            assert(consistent);
            assert(denominator.sign() > 0);
            if (!consistent)
                return true;
            COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("affine-consistent", nullity);
            if (is_nonpositive(affine_particular_))
                return false;

            affine_products_.resize(outside_indices_.size(), 1);
            for (size_t outside = 0; outside < outside_indices_.size(); ++outside) {
                timeout_checkpoint();
                affine_products_(outside, 0).set_zero();
                for (size_t local = 0; local < indices_.size(); ++local)
                    affine_products_(outside, 0).addmul(matrix(outside_indices_[outside], indices_[local]), affine_particular_(local, 0));
            }

            if (nullity == 1) {
                build_projected_outside_rows(matrix);
                return search_affine_line();
            }
            for (size_t row = 0; row < affine_products_.rows(); ++row)
                if (affine_products_(row, 0).sign() < 0)
                    return true;
            return add_affine_certificate(affine_particular_);
        }

        void set_negative_quotient(signed_ratio& ratio, integer::const_reference base, integer::const_reference direction)
        {
            assert(!direction.is_zero());
            ratio.denominator.set_abs(direction);
            ratio.numerator = base;
            if (direction.sign() > 0)
                ratio.numerator.negate();
        }

        int compare_ratios(const signed_ratio& left, const signed_ratio& right)
        {
            ratio_left_.set_product(left.numerator, right.denominator);
            ratio_right_.set_product(right.numerator, left.denominator);
            return ratio_left_.compare(ratio_right_);
        }

        bool ratio_is_feasible(const signed_ratio& ratio)
        {
            return (!has_affine_lower_ || compare_ratios(ratio, affine_lower_) >= 0)
                && (!has_affine_upper_ || compare_ratios(ratio, affine_upper_) <= 0);
        }

        bool search_affine_line()
        {
            has_affine_lower_ = false;
            has_affine_upper_ = false;
            for (size_t row = 0; row < projected_.rows(); ++row) {
                timeout_checkpoint();
                const auto direction = projected_(row, 0);
                const auto base = affine_products_(row, 0);
                if (direction.is_zero()) {
                    if (base.sign() < 0)
                        return true;
                    continue;
                }

                signed_ratio bound;
                set_negative_quotient(bound, base, direction);
                if (direction.sign() > 0) {
                    if (!has_affine_lower_ || compare_ratios(bound, affine_lower_) > 0)
                        affine_lower_ = std::move(bound);
                    has_affine_lower_ = true;
                } else {
                    if (!has_affine_upper_ || compare_ratios(bound, affine_upper_) < 0)
                        affine_upper_ = std::move(bound);
                    has_affine_upper_ = true;
                }
            }
            if (has_affine_lower_ && has_affine_upper_ && compare_ratios(affine_lower_, affine_upper_) > 0)
                return true;

            affine_breakpoints_.clear();
            affine_breakpoints_.reserve(indices_.size());
            for (size_t row = 0; row < indices_.size(); ++row) {
                if (root_basis_(row, 0).is_zero())
                    continue;
                signed_ratio breakpoint;
                set_negative_quotient(breakpoint, affine_particular_(row, 0), root_basis_(row, 0));
                if (ratio_is_feasible(breakpoint))
                    affine_breakpoints_.push_back(std::move(breakpoint));
            }

            const auto less = [&](const signed_ratio& left, const signed_ratio& right) { return compare_ratios(left, right) < 0; };
            std::sort(affine_breakpoints_.begin(), affine_breakpoints_.end(), less);

            signed_ratio selected;
            if (affine_breakpoints_.empty()) {
                selected.numerator.set_zero();
                selected.denominator.set_one();
                if (!ratio_is_feasible(selected))
                    selected = has_affine_lower_ ? affine_lower_ : affine_upper_;
            } else {
                size_t best = 0;
                size_t best_count = 0;
                for (size_t first = 0; first < affine_breakpoints_.size();) {
                    size_t last = first + 1;
                    while (last < affine_breakpoints_.size() && compare_ratios(affine_breakpoints_[first], affine_breakpoints_[last]) == 0)
                        ++last;
                    if (last - first > best_count) {
                        best = first;
                        best_count = last - first;
                    }
                    first = last;
                }
                selected = affine_breakpoints_[best];
            }

            candidate_.resize(indices_.size(), 1);
            for (size_t row = 0; row < indices_.size(); ++row) {
                candidate_(row, 0).set_product(affine_particular_(row, 0), selected.denominator);
                candidate_(row, 0).addmul(root_basis_(row, 0), selected.numerator);
            }
            return add_affine_certificate(candidate_);
        }

        bool add_affine_certificate(const matrix_integer& vector)
        {
            if (is_nonpositive(vector))
                return false;

            add_known_ceiling_certificate(indices_.size() + outside_indices_.size(), vector);
            COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("affine-certificate", support_size(vector));
            return true;
        }

        static bool is_nonpositive(const matrix_integer& vector)
        {
            for (size_t row = 0; row < vector.rows(); ++row)
                if (vector(row, 0).sign() > 0)
                    return false;
            return true;
        }

        bool search_root_kernel(const matrix_integer& matrix)
        {
            const size_t root_dimension = indices_.size();
            const size_t nullity = root_dimension - factorization_.rank();
            assert(nullity > 1);

            build_projected_outside_rows(matrix);
            const size_t persistent_nullity = exact_nullspace(projected_, persistent_basis_);
            COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("persistent-kernel", persistent_nullity);
            if (persistent_nullity != 0)
                return add_persistent_circuit(matrix);
            reduce_projected_rows();
            return enumerate_active_rays(matrix, nullity);
        }

        void build_projected_outside_rows(const matrix_integer& matrix)
        {
            COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("projected-build", root_basis_.cols());
            projected_.resize(outside_indices_.size(), root_basis_.cols());
            for (size_t outside = 0; outside < outside_indices_.size(); ++outside) {
                timeout_checkpoint();
                for (size_t basis_column = 0; basis_column < root_basis_.cols(); ++basis_column) {
                    projected_(outside, basis_column).set_zero();
                    for (size_t root_row = 0; root_row < indices_.size(); ++root_row)
                        projected_(outside, basis_column)
                            .addmul(matrix(outside_indices_[outside], indices_[root_row]), root_basis_(root_row, basis_column));
                }
            }
        }

        void reduce_projected_rows()
        {
            nonzero_projected_rows_.clear();
            projected_row_contents_.clear();
            nonzero_projected_rows_.reserve(projected_.rows());
            projected_row_contents_.reserve(projected_.rows());

            for (size_t row = 0; row < projected_.rows(); ++row) {
                integer content;
                integer next;
                for (size_t column = 0; column < projected_.cols(); ++column) {
                    fmpz_gcd(next.native_handle(), content.native_handle(), projected_(row, column).native_handle());
                    content.swap(next);
                }
                if (content.is_zero())
                    continue;
                nonzero_projected_rows_.push_back(row);
                projected_row_contents_.push_back(std::move(content));
            }

            normalized_projected_.resize(nonzero_projected_rows_.size(), projected_.cols());
            for (size_t destination = 0; destination < nonzero_projected_rows_.size(); ++destination) {
                const size_t source = nonzero_projected_rows_[destination];
                for (size_t column = 0; column < projected_.cols(); ++column) {
                    normalized_projected_(destination, column) = projected_(source, column);
                    normalized_projected_(destination, column).divide_exact(projected_row_contents_[destination]);
                }
            }

            projected_row_order_.resize(normalized_projected_.rows());
            std::iota(projected_row_order_.begin(), projected_row_order_.end(), size_t { 0 });
            const auto less = [&](size_t left, size_t right) {
                for (size_t column = 0; column < normalized_projected_.cols(); ++column) {
                    const int comparison = normalized_projected_(left, column).compare(normalized_projected_(right, column));
                    if (comparison != 0)
                        return comparison < 0;
                }
                return false;
            };
            const auto equal = [&](size_t left, size_t right) {
                for (size_t column = 0; column < normalized_projected_.cols(); ++column)
                    if (normalized_projected_(left, column).compare(normalized_projected_(right, column)) != 0)
                        return false;
                return true;
            };
            std::sort(projected_row_order_.begin(), projected_row_order_.end(), less);
            projected_row_order_.erase(
                std::unique(projected_row_order_.begin(), projected_row_order_.end(), equal), projected_row_order_.end());

            constraint_rows_.resize(projected_row_order_.size(), projected_.cols());
            for (size_t destination = 0; destination < projected_row_order_.size(); ++destination)
                for (size_t column = 0; column < projected_.cols(); ++column)
                    constraint_rows_(destination, column) = normalized_projected_(projected_row_order_[destination], column);
            COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("projected-constraints", constraint_rows_.rows());
        }

        bool add_persistent_circuit(const matrix_integer& matrix)
        {
            coefficient_ray_.resize(root_basis_.cols(), 1);
            for (size_t row = 0; row < coefficient_ray_.rows(); ++row)
                coefficient_ray_(row, 0) = persistent_basis_(row, 0);
            materialize_root_vector(coefficient_ray_, candidate_);

            circuit_locals_.clear();
            for (size_t local = 0; local < candidate_.rows(); ++local)
                if (!candidate_(local, 0).is_zero())
                    circuit_locals_.push_back(local);

            for (size_t position = 0; position < circuit_locals_.size();) {
                timeout_checkpoint();
                if (circuit_locals_.size() == 1)
                    break;
                copy_tall_columns_without(matrix, position);
                if (static_cast<size_t>(fmpz_mat_rank(tall_columns_.native_handle())) < tall_columns_.cols()) {
                    circuit_locals_.erase(circuit_locals_.begin() + static_cast<std::ptrdiff_t>(position));
                } else {
                    ++position;
                }
            }

            copy_tall_columns(matrix);
            [[maybe_unused]] const size_t circuit_nullity = exact_nullspace(tall_columns_, circuit_basis_);
            assert(circuit_nullity == 1);
            candidate_.resize(indices_.size(), 1);
            fmpz_mat_zero(candidate_.native_handle());
            for (size_t position = 0; position < circuit_locals_.size(); ++position)
                candidate_(circuit_locals_[position], 0) = circuit_basis_(position, 0);
            canonicalize_ray(candidate_);

            if (!record_one_sided_zero(candidate_))
                return false;
            add_known_ceiling_certificate(matrix.rows(), candidate_);
            COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("persistent-certificate", circuit_locals_.size());
            return true;
        }

        bool enumerate_active_rays(const matrix_integer& matrix, size_t nullity)
        {
            if (nullity == 2)
                return enumerate_planar_rays(matrix);

            build_active_hyperplanes();
            const size_t chosen = nullity - 1;
            if (chosen > active_hyperplane_order_.size())
                return true;

            active_rows_.resize(chosen, active_hyperplanes_.cols());
            combination_.resize(chosen);
            std::iota(combination_.begin(), combination_.end(), size_t { 0 });
            seen_rays_.clear();

            do {
                timeout_checkpoint();
                COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("active-set", nullity);
                for (size_t row = 0; row < chosen; ++row)
                    for (size_t column = 0; column < active_hyperplanes_.cols(); ++column)
                        active_rows_(row, column) = active_hyperplanes_(active_hyperplane_order_[combination_[row]], column);

                if (exact_nullspace(active_rows_, active_basis_) == 1) {
                    coefficient_ray_.resize(nullity, 1);
                    for (size_t row = 0; row < nullity; ++row)
                        coefficient_ray_(row, 0) = active_basis_(row, 0);
                    canonicalize_ray(coefficient_ray_);
                    if (!seen_rays_.insert(ray_key(coefficient_ray_)).second) {
                        COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("duplicate-ray", nullity);
                    } else if (!consider_active_ray(matrix)) {
                        return false;
                    }
                }

            } while (advance_combination(combination_, active_hyperplane_order_.size()));
            return true;
        }

        void build_active_hyperplanes()
        {
            active_hyperplanes_.resize(constraint_rows_.rows(), constraint_rows_.cols());
            for (size_t row = 0; row < constraint_rows_.rows(); ++row) {
                int orientation = 0;
                for (size_t column = 0; column < constraint_rows_.cols() && orientation == 0; ++column)
                    orientation = constraint_rows_(row, column).sign();
                assert(orientation != 0);

                for (size_t column = 0; column < constraint_rows_.cols(); ++column) {
                    active_hyperplanes_(row, column) = constraint_rows_(row, column);
                    if (orientation < 0)
                        active_hyperplanes_(row, column).negate();
                }
            }

            active_hyperplane_order_.resize(active_hyperplanes_.rows());
            std::iota(active_hyperplane_order_.begin(), active_hyperplane_order_.end(), size_t { 0 });
            const auto less = [&](size_t left, size_t right) {
                for (size_t column = 0; column < active_hyperplanes_.cols(); ++column) {
                    const int comparison = active_hyperplanes_(left, column).compare(active_hyperplanes_(right, column));
                    if (comparison != 0)
                        return comparison < 0;
                }
                return false;
            };
            const auto equal = [&](size_t left, size_t right) {
                for (size_t column = 0; column < active_hyperplanes_.cols(); ++column)
                    if (active_hyperplanes_(left, column).compare(active_hyperplanes_(right, column)) != 0)
                        return false;
                return true;
            };
            std::sort(active_hyperplane_order_.begin(), active_hyperplane_order_.end(), less);
            active_hyperplane_order_.erase(
                std::unique(active_hyperplane_order_.begin(), active_hyperplane_order_.end(), equal), active_hyperplane_order_.end());
            COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("active-hyperplanes", active_hyperplane_order_.size());
        }

        static int angular_half(integer::const_reference first, integer::const_reference second) noexcept
        {
            return second.sign() > 0 || (second.is_zero() && first.sign() >= 0) ? 0 : 1;
        }

        int cross_sign(size_t left, size_t right)
        {
            cross_left_.set_product(constraint_rows_(left, 0), constraint_rows_(right, 1));
            cross_right_.set_product(constraint_rows_(left, 1), constraint_rows_(right, 0));
            return cross_left_.compare(cross_right_);
        }

        void set_planar_normal(size_t row, bool counterclockwise)
        {
            coefficient_ray_.resize(2, 1);
            if (counterclockwise) {
                coefficient_ray_(0, 0) = constraint_rows_(row, 1);
                coefficient_ray_(0, 0).negate();
                coefficient_ray_(1, 0) = constraint_rows_(row, 0);
            } else {
                coefficient_ray_(0, 0) = constraint_rows_(row, 1);
                coefficient_ray_(1, 0) = constraint_rows_(row, 0);
                coefficient_ray_(1, 0).negate();
            }
            canonicalize_ray(coefficient_ray_);
        }

        bool enumerate_planar_rays(const matrix_integer& matrix)
        {
            assert(constraint_rows_.cols() == 2);
            assert(constraint_rows_.rows() >= 1);

            angular_order_.resize(constraint_rows_.rows());
            std::iota(angular_order_.begin(), angular_order_.end(), size_t { 0 });
            const auto angular_less = [&](size_t left, size_t right) {
                const int left_half = angular_half(constraint_rows_(left, 0), constraint_rows_(left, 1));
                const int right_half = angular_half(constraint_rows_(right, 0), constraint_rows_(right, 1));
                if (left_half != right_half)
                    return left_half < right_half;
                const int cross = cross_sign(left, right);
                if (cross != 0)
                    return cross > 0;
                return constraint_rows_(left, 0).compare(constraint_rows_(right, 0)) < 0;
            };
            std::sort(angular_order_.begin(), angular_order_.end(), angular_less);

            // The generic q=2 path inspected the perpendicular of every projected row.
            // Preserve those one-sided-zero decisions, but postpone product scans until
            // the two actual cone boundaries are known.
            seen_rays_.clear();
            for (const size_t row : angular_order_) {
                timeout_checkpoint();
                set_planar_normal(row, true);
                if (!seen_rays_.insert(ray_key(coefficient_ray_)).second)
                    continue;
                materialize_root_vector(coefficient_ray_, candidate_);
                if (!record_one_sided_zero(candidate_))
                    return false;
            }

            size_t first_boundary = constraint_rows_.rows();
            size_t last_boundary = constraint_rows_.rows();
            for (size_t start = 0; start < angular_order_.size(); ++start) {
                const size_t end = (start + angular_order_.size() - 1) % angular_order_.size();
                if (cross_sign(angular_order_[start], angular_order_[end]) < 0)
                    continue;
                first_boundary = angular_order_[start];
                last_boundary = angular_order_[end];
                break;
            }
            if (first_boundary == constraint_rows_.rows())
                return true;

            std::unordered_set<std::string> boundary_rays;
            set_planar_normal(first_boundary, true);
            boundary_rays.insert(ray_key(coefficient_ray_));
            if (!consider_active_ray(matrix))
                return false;

            set_planar_normal(last_boundary, false);
            if (!boundary_rays.insert(ray_key(coefficient_ray_)).second) {
                COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("duplicate-ray", 2);
                return true;
            }
            return consider_active_ray(matrix);
        }

        bool consider_active_ray(const matrix_integer& matrix)
        {
            materialize_root_vector(coefficient_ray_, candidate_);
            bool products_nonnegative = true;
            bool products_nonpositive = true;
            for (size_t row = 0; row < constraint_rows_.rows(); ++row) {
                timeout_checkpoint();
                product_.set_zero();
                for (size_t column = 0; column < constraint_rows_.cols(); ++column)
                    product_.addmul(constraint_rows_(row, column), coefficient_ray_(column, 0));
                products_nonnegative &= product_.sign() >= 0;
                products_nonpositive &= product_.sign() <= 0;
            }

            bool has_positive = false;
            bool has_negative = false;
            for (size_t row = 0; row < candidate_.rows(); ++row) {
                has_positive |= candidate_(row, 0).sign() > 0;
                has_negative |= candidate_(row, 0).sign() < 0;
            }
            if ((!has_positive || !has_negative) && !record_one_sided_zero(candidate_))
                return false;

            if (products_nonnegative && has_positive) {
                add_known_ceiling_certificate(matrix.rows(), candidate_);
                COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("active-ray-certificate", support_size(candidate_));
            } else if (products_nonpositive && has_negative) {
                candidate_.negate();
                add_known_ceiling_certificate(matrix.rows(), candidate_);
                COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("active-ray-certificate", support_size(candidate_));
            }
            return true;
        }

        void materialize_root_vector(const matrix_integer& coefficients, matrix_integer& result)
        {
            result.resize(root_basis_.rows(), 1);
            for (size_t row = 0; row < root_basis_.rows(); ++row) {
                result(row, 0).set_zero();
                for (size_t column = 0; column < root_basis_.cols(); ++column)
                    result(row, 0).addmul(root_basis_(row, column), coefficients(column, 0));
            }
        }

        bool record_nonnegative_zero()
        {
            if (classification_ != nullptr)
                classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive)
                return false;
            return true;
        }

        bool record_one_sided_zero(const matrix_integer& vector)
        {
            bool has_positive = false;
            bool has_negative = false;
            for (size_t row = 0; row < vector.rows(); ++row) {
                has_positive |= vector(row, 0).sign() > 0;
                has_negative |= vector(row, 0).sign() < 0;
            }
            return (has_positive && has_negative) || record_nonnegative_zero();
        }

        void add_ceiling_certificate(const matrix_integer& matrix, const matrix_integer& vector)
        {
            size_t support_row = 0;
            for (size_t row = 0; row < matrix.rows(); ++row) {
                if (support_row < indices_.size() && indices_[support_row] == row) {
                    ++support_row;
                    continue;
                }
                timeout_checkpoint();
                product_.set_zero();
                for (size_t local = 0; local < indices_.size(); ++local)
                    product_.addmul(matrix(row, indices_[local]), vector(local, 0));
                if (product_.sign() < 0) {
                    COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("discard-certificate", indices_.size());
                    return;
                }
            }
            add_known_ceiling_certificate(matrix.rows(), vector);
        }

        void add_known_ceiling_certificate(size_t matrix_dimension, const matrix_integer& vector)
        {
            support lower = support_context_.make();
            size_t lower_size = 0;
            for (size_t local = 0; local < indices_.size(); ++local) {
                if (vector(local, 0).is_zero())
                    continue;
                support_context_.set(lower, indices_[local]);
                ++lower_size;
            }

            supports_.add_forbidden(std::move(lower));
            diagnostics_.certificate(matrix_dimension - lower_size);
            COPOSIT_AFFINE_COMPANION_DICKINSON_DIAGNOSTICS("ceiling-certificate", lower_size);
        }

        static size_t support_size(const matrix_integer& vector)
        {
            size_t result = 0;
            for (size_t row = 0; row < vector.rows(); ++row)
                result += !vector(row, 0).is_zero();
            return result;
        }

        void copy_tall_columns_without(const matrix_integer& matrix, size_t omitted_position)
        {
            tall_columns_.resize(matrix.rows(), circuit_locals_.size() - 1);
            size_t destination = 0;
            for (size_t position = 0; position < circuit_locals_.size(); ++position) {
                if (position == omitted_position)
                    continue;
                const size_t source_column = indices_[circuit_locals_[position]];
                for (size_t row = 0; row < matrix.rows(); ++row)
                    tall_columns_(row, destination) = matrix(row, source_column);
                ++destination;
            }
        }

        void copy_tall_columns(const matrix_integer& matrix)
        {
            tall_columns_.resize(matrix.rows(), circuit_locals_.size());
            for (size_t position = 0; position < circuit_locals_.size(); ++position) {
                const size_t source_column = indices_[circuit_locals_[position]];
                for (size_t row = 0; row < matrix.rows(); ++row)
                    tall_columns_(row, position) = matrix(row, source_column);
            }
        }

        static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
        {
            for (size_t row = 0; row < indices.size(); ++row) {
                timeout_checkpoint();
                for (size_t column = 0; column <= row; ++column)
                    principal(row, column) = matrix(indices[row], indices[column]);
            }
        }

        support_context support_context_;
        fraction_free_ldlt_factorization factorization_;
        matrix_integer principal_;
        matrix_integer solution_;
        matrix_integer root_basis_;
        matrix_integer projected_;
        matrix_integer affine_particular_;
        matrix_integer affine_products_;
        matrix_integer normalized_projected_;
        matrix_integer constraint_rows_;
        matrix_integer active_hyperplanes_;
        matrix_integer persistent_basis_;
        matrix_integer coefficient_ray_;
        matrix_integer candidate_;
        matrix_integer active_rows_;
        matrix_integer active_basis_;
        matrix_integer tall_columns_;
        matrix_integer circuit_basis_;
        integer product_;
        integer cross_left_;
        integer cross_right_;
        integer affine_sum_;
        integer ratio_left_;
        integer ratio_right_;
        std::vector<size_t> indices_;
        std::vector<size_t> outside_indices_;
        std::vector<size_t> nonzero_projected_rows_;
        std::vector<integer> projected_row_contents_;
        std::vector<size_t> projected_row_order_;
        std::vector<size_t> active_hyperplane_order_;
        std::vector<size_t> angular_order_;
        std::vector<size_t> combination_;
        std::vector<size_t> circuit_locals_;
        std::vector<signed_ratio> affine_breakpoints_;
        std::unordered_set<std::string> seen_rays_;
        signed_ratio affine_lower_;
        signed_ratio affine_upper_;
        bool has_affine_lower_ = false;
        bool has_affine_upper_ = false;
        const copositivity_mode mode_;
        copositivity_classification* classification_ = nullptr;
        diagnostics::tracker diagnostics_;
        support_generator supports_;
    };

} // namespace

bool solve(const matrix_integer& matrix, copositivity_mode mode)
{
    timeout_checkpoint();
    return dickinson_checker(matrix.rows(), mode).check(matrix);
}

copositivity_classification classify(const matrix_integer& matrix)
{
    timeout_checkpoint();
    copositivity_classification result { true, true };
    if (!dickinson_checker(matrix.rows(), result).check(matrix))
        result = { false, false };
    return result;
}

#ifdef COPOSIT_AFFINE_COMPANION_DICKINSON_TESTING
bool affine_companion_search_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive).search_kernel_for_test(matrix, indices);
}

bool affine_companion_strict_search_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::strictly_copositive).search_kernel_for_test(matrix, indices);
}

bool affine_companion_affine_search_for_test(const matrix_integer& matrix, const std::vector<size_t>& indices)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive).search_affine_for_test(matrix, indices);
}

std::vector<uint64_t> affine_companion_generated_masks(size_t dimension, uint64_t forbidden_trigger)
{
    assert(dimension <= 64);
    support_generator generator(dimension);
    std::vector<uint64_t> result;
    generator.generate([&](const support& current, size_t) {
        const uint64_t mask = generator.mask_for_testing(current);
        result.push_back(mask);
        if (mask == forbidden_trigger)
            generator.add_forbidden_copy_for_testing(current);
        return true;
    });
    return result;
}
#endif

} // namespace coposit::model
