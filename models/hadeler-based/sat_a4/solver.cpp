#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cadical.hpp>

#include "source_diagnostics.hpp"
#include "tiny_monotone_extension.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

struct coverage_score {
    size_t width = 0;
    size_t upper_size = 0;
};

bool better_score(const coverage_score& candidate, const coverage_score& current) noexcept
{
    return candidate.upper_size > current.upper_size
        || (candidate.upper_size == current.upper_size && candidate.width > current.width);
}

#ifdef COPOSIT_SAT_A4_TESTING
size_t last_monotone_lp_attempt_count = 0;
size_t last_monotone_lp_feasible_count = 0;
size_t last_monotone_extension_count = 0;
size_t last_monotone_exact_rejection_count = 0;
size_t last_monotone_incumbent_upper_size = 0;
size_t last_monotone_final_upper_size = 0;
std::vector<bool> last_monotone_incumbent_upper;
#endif

class timeout_terminator final : public CaDiCaL::Terminator {
public:
    bool terminate() override { return timeout_pending(); }
};

class interval_sat {
public:
    explicit interval_sat(size_t dimension)
        : dimension_(dimension)
    {
        if (dimension_ > static_cast<size_t>(std::numeric_limits<int>::max() - 1))
            throw std::overflow_error("SAT variable count exceeds CaDiCaL's integer literal range");
        next_variable_ = static_cast<int>(dimension_) + 1;

        if (!solver_.configure("sat")) throw std::runtime_error("CaDiCaL lacks its satisfiable-instance configuration");
        if (!solver_.set("ilb", 2)) throw std::runtime_error("CaDiCaL lacks incremental lazy backtracking");
        solver_.connect_terminator(&terminator_);
        std::vector<int> wires;
        size_t padded_dimension = 1;
        while (padded_dimension < dimension_) {
            if (padded_dimension > static_cast<size_t>(std::numeric_limits<int>::max()) / 2)
                throw std::overflow_error("SAT cardinality network is too large");
            padded_dimension *= 2;
        }
        wires.reserve(padded_dimension);
        for (size_t index = 0; index < dimension_; ++index) wires.push_back(variable(index));

        if (padded_dimension != dimension_) {
            const int constant_false = new_variable();
            add_clause({-constant_false});
            wires.resize(padded_dimension, constant_false);
        }

        bitonic_sort(wires, 0, wires.size(), true);
        cardinality_outputs_.assign(wires.begin(), wires.begin() + dimension_);
    }

    void start_cardinality(size_t cardinality) noexcept { cardinality_ = cardinality; }

    bool take_first(std::vector<size_t>& indices)
    {
        assert(cardinality_ >= 1 && cardinality_ <= dimension_);
        solver_.assume(cardinality_outputs_[cardinality_ - 1]);
        if (cardinality_ < dimension_) solver_.assume(-cardinality_outputs_[cardinality_]);

        const int status = solver_.solve();
        if (status == CaDiCaL::UNSATISFIABLE) return false;
        if (status != CaDiCaL::SATISFIABLE) {
            timeout_checkpoint();
            throw std::runtime_error("CaDiCaL returned an inconclusive result without a coposit timeout");
        }

        indices.clear();
        for (size_t index = 0; index < dimension_; ++index)
            if (solver_.val(variable(index)) > 0) indices.push_back(index);
        assert(indices.size() == cardinality_);
        return true;
    }

    void add_interval(const support& lower, const support& upper)
    {
        size_t upper_size = 0;
#ifdef COPOSIT_SAT_A4_TESTING
        last_interval_clause_size_ = 0;
#endif
        for (size_t index = 0; index < dimension_; ++index) {
            const bool in_upper = upper.contains(index);
            if (in_upper) ++upper_size;
            if (lower.contains(index)) {
                solver_.add(-variable(index));
#ifdef COPOSIT_SAT_A4_TESTING
                ++last_interval_clause_size_;
#endif
            } else if (!in_upper) {
                solver_.add(variable(index));
#ifdef COPOSIT_SAT_A4_TESTING
                ++last_interval_clause_size_;
#endif
            }
        }
        if (upper_size < dimension_) {
            solver_.add(cardinality_outputs_[upper_size]);
#ifdef COPOSIT_SAT_A4_TESTING
            ++last_interval_clause_size_;
#endif
        }
        solver_.add(0);
        ++interval_count_;
    }

    size_t interval_count() const noexcept { return interval_count_; }
#ifdef COPOSIT_SAT_A4_TESTING
    size_t last_interval_clause_size() const noexcept { return last_interval_clause_size_; }
#endif

private:
    int variable(size_t index) const noexcept
    {
        return static_cast<int>(index) + 1;
    }

    int new_variable()
    {
        if (next_variable_ == std::numeric_limits<int>::max())
            throw std::overflow_error("SAT cardinality network exceeds CaDiCaL's integer literal range");
        if ((next_variable_ & 4095) == 0) timeout_checkpoint();
        return next_variable_++;
    }

    void add_clause(std::initializer_list<int> literals)
    {
        for (const int literal : literals) solver_.add(literal);
        solver_.add(0);
    }

    std::pair<int, int> comparator(int first, int second)
    {
        const int high = new_variable();
        const int low = new_variable();
        add_clause({-first, high});
        add_clause({-second, high});
        add_clause({first, second, -high});
        add_clause({first, -low});
        add_clause({second, -low});
        add_clause({-first, -second, low});
        return {high, low};
    }

    void compare_exchange(std::vector<int>& wires, size_t first, size_t second, bool descending)
    {
        const auto [high, low] = comparator(wires[first], wires[second]);
        wires[first] = descending ? high : low;
        wires[second] = descending ? low : high;
    }

    void bitonic_merge(std::vector<int>& wires, size_t first, size_t count, bool descending)
    {
        if (count < 2) return;
        const size_t half = count / 2;
        for (size_t index = first; index < first + half; ++index)
            compare_exchange(wires, index, index + half, descending);
        bitonic_merge(wires, first, half, descending);
        bitonic_merge(wires, first + half, half, descending);
    }

    void bitonic_sort(std::vector<int>& wires, size_t first, size_t count, bool descending)
    {
        if (count < 2) return;
        const size_t half = count / 2;
        bitonic_sort(wires, first, half, !descending);
        bitonic_sort(wires, first + half, half, descending);
        bitonic_merge(wires, first, count, descending);
    }

    size_t dimension_;
    int next_variable_ = 1;
    size_t cardinality_ = 0;
    size_t interval_count_ = 0;
#ifdef COPOSIT_SAT_A4_TESTING
    size_t last_interval_clause_size_ = 0;
#endif
    timeout_terminator terminator_;
    CaDiCaL::Solver solver_;
    std::vector<int> cardinality_outputs_;
};

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
#ifdef COPOSIT_SAT_A4_TESTING
        reset_test_counters();
#endif
        supports_.emplace(matrix.rows());
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            diagnostics_.stage(subset_dimension);
            supports_->start_cardinality(subset_dimension);
            while (supports_->take_first(indices_)) {
                timeout_checkpoint();
                diagnostics_.visit_support();
                diagnostics_.secondary();
                COPOSIT_SAT_A4_DIAGNOSTICS("process", subset_dimension);
                if (!process_subset(matrix)) {
                    diagnostics_.finish();
#ifdef COPOSIT_SAT_A4_TESTING
                    publish_test_counters();
#endif
                    return false;
                }
            }
        }

        diagnostics_.finish();
#ifdef COPOSIT_SAT_A4_TESTING
        publish_test_counters();
#endif
        return true;
    }

#ifdef COPOSIT_SAT_A4_TESTING
    bool optimize_support_for_testing(
        const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper)
    {
        reset_test_counters();
        indices_ = indices;
        captured_lower_ = &lower;
        captured_upper_ = &upper;
        const bool result = process_subset(matrix);
        captured_lower_ = nullptr;
        captured_upper_ = nullptr;
        publish_test_counters();
        return result;
    }
#endif

private:
    bool process_subset(const matrix_integer& matrix)
    {
        const size_t dimension = indices_.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices_, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
        if (singular && diagnostics_.active()) diagnostics_.singular_support(dimension - factorization_.rank());
        if (singular) return process_singular_subset(matrix);
        return process_nonsingular_subset(matrix);
    }

    bool process_singular_subset(const matrix_integer& matrix)
    {
        factorization_.one_nullspace_vector(solution_, principal_);

        bool has_positive_entry = false;
        bool all_nonnegative = true;
        for (size_t row = 0; row < solution_.rows(); ++row) {
            has_positive_entry |= solution_(row, 0).sign() > 0;
            all_nonnegative &= solution_(row, 0).sign() >= 0;
        }
        if (!has_positive_entry) {
            solution_.negate();
            all_nonnegative = true;
        }
        if (all_nonnegative) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

        calculate_product(matrix, solution_, 0, product_);
        return add_certificate();
    }

    bool process_nonsingular_subset(const matrix_integer& matrix)
    {
        const size_t dimension = indices_.size();
        for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);
        if (all_nonpositive(solution_, 0)) return false;

        calculate_nonsingular_product(matrix, solution_, 0, denominator, product_);
        current_score_ = score(solution_, 0, product_);
        if (dimension > 1 && current_score_.upper_size < matrix.rows()) {
            directions_.resize(dimension, dimension);
            for (size_t row = 0; row < dimension; ++row) {
                for (size_t column = 0; column < dimension; ++column) {
                    if (row == column) directions_(row, column).set_one();
                    else directions_(row, column).set_zero();
                }
            }

            integer direction_denominator;
            factorization_.solve_inplace(directions_, direction_denominator, principal_);
            assert(direction_denominator.compare(denominator) == 0);

            direction_products_.resize(matrix.rows(), dimension);
            for (size_t direction = 0; direction < dimension; ++direction)
                calculate_nonsingular_product(
                    matrix, directions_, direction, direction_denominator, direction_products_, direction);

            if (!try_monotone_enlargement()) return false;
        }

        return add_certificate();
    }

    enum class exact_lp_candidate_result { unchanged, improved, negative_witness };

    bool try_monotone_enlargement()
    {
        timeout_checkpoint();
        const size_t support_dimension = indices_.size();
        const size_t outside_dimension = product_.size() - support_dimension;
        if (outside_dimension == 0 || !lp_problem_fits(support_dimension, outside_dimension)) return true;

        lp_outside_rows_.clear();
        lp_scaled_rows_.resize(outside_dimension);
        for (auto& row : lp_scaled_rows_) {
            row.resize(support_dimension);
            std::fill(row.begin(), row.end(), 0.0);
        }
        lp_outside_rows_.reserve(outside_dimension);
        size_t outside = 0;
        size_t local_row = 0;
        for (size_t row = 0; row < product_.size(); ++row) {
            if (local_row < indices_.size() && row == indices_[local_row]) {
                ++local_row;
                continue;
            }
            if ((outside & 63U) == 0) timeout_checkpoint();
            lp_outside_rows_.push_back(row);
            slong maximum_exponent = std::numeric_limits<slong>::min();
            for (size_t column = 0; column < support_dimension; ++column) {
                if (direction_products_(row, column).is_zero()) continue;
                slong exponent = 0;
                static_cast<void>(direction_products_(row, column).to_dbl_2exp(exponent));
                maximum_exponent = std::max(maximum_exponent, exponent);
            }
            if (maximum_exponent != std::numeric_limits<slong>::min()) {
                for (size_t column = 0; column < support_dimension; ++column) {
                    if (direction_products_(row, column).is_zero()) continue;
                    slong exponent = 0;
                    const double mantissa = direction_products_(row, column).to_dbl_2exp(exponent);
                    const slong difference = exponent - maximum_exponent;
                    lp_scaled_rows_[outside][column] =
                        difference < -1074 ? 0.0 : std::scalbn(mantissa, static_cast<int>(difference));
                }
            }
            ++outside;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
#ifdef COPOSIT_SAT_A4_TESTING
        monotone_incumbent_upper_size_ = current_score_.upper_size;
        last_monotone_incumbent_upper.assign(product_.size(), false);
        for (size_t row = 0; row < product_.size(); ++row)
            last_monotone_incumbent_upper[row] = product_[row].sign() >= 0;
#endif

        while (current_score_.upper_size < product_.size()) {
            lp_required_rows_.clear();
            for (size_t row = 0; row < outside_dimension; ++row)
                if (product_[lp_outside_rows_[row]].sign() >= 0) lp_required_rows_.push_back(row);

            bool enlarged = false;
            const size_t preserved_count = lp_required_rows_.size();
            for (size_t target = 0; target < outside_dimension; ++target) {
                timeout_checkpoint();
                if (product_[lp_outside_rows_[target]].sign() >= 0) continue;
                lp_required_rows_.resize(preserved_count);
                lp_required_rows_.push_back(target);
                detail::tiny_monotone_extension feasibility(lp_scaled_rows_, lp_required_rows_, deadline);
                const detail::monotone_extension_result proposal = feasibility.solve();
#ifdef COPOSIT_SAT_A4_TESTING
                ++monotone_lp_attempt_count_;
#endif
                if (proposal.interrupted) {
#ifdef COPOSIT_SAT_A4_TESTING
                    monotone_final_upper_size_ = current_score_.upper_size;
#endif
                    return true;
                }
                if (!proposal.feasible) continue;
#ifdef COPOSIT_SAT_A4_TESTING
                ++monotone_lp_feasible_count_;
#endif
                const exact_lp_candidate_result result = consider_lp_point(proposal.point, lp_required_rows_);
                if (result == exact_lp_candidate_result::negative_witness) return false;
                if (result == exact_lp_candidate_result::improved) {
#ifdef COPOSIT_SAT_A4_TESTING
                    ++monotone_extension_count_;
#endif
                    enlarged = true;
                    break;
                }
#ifdef COPOSIT_SAT_A4_TESTING
                ++monotone_exact_rejection_count_;
#endif
            }
            if (!enlarged) break;
        }
#ifdef COPOSIT_SAT_A4_TESTING
        monotone_final_upper_size_ = current_score_.upper_size;
#endif
        return true;
    }

    exact_lp_candidate_result consider_lp_point(const std::vector<double>& point, const std::vector<size_t>& required_outside_rows)
    {
        lp_coefficients_.resize(point.size());
        lp_solution_.resize(point.size(), 1);
        lp_product_.resize(product_.size());
        coverage_score selected_score = current_score_;
        bool selected = false;
        const double maximum = *std::max_element(point.begin(), point.end());
        if (!(maximum > 0.0) || !std::isfinite(maximum)) return exact_lp_candidate_result::unchanged;
        for (const uint64_t scale :
             {uint64_t{1000000}, uint64_t{1000000000}, uint64_t{1000000000000}, uint64_t{1000000000000000}}) {
            bool positive = true;
            for (size_t column = 0; column < point.size(); ++column) {
                const double value = point[column] / maximum;
                if (!(value > 0.0) || !std::isfinite(value)) {
                    positive = false;
                    break;
                }
                const uint64_t coefficient = static_cast<uint64_t>(std::llround(value * static_cast<double>(scale)));
                if (coefficient == 0) {
                    positive = false;
                    break;
                }
                fmpz_set_ui(lp_coefficients_[column].native_handle(), static_cast<ulong>(coefficient));
            }
            if (!positive) continue;

            multiply_directions(directions_, direction_products_, lp_coefficients_, lp_solution_, lp_product_);
            if (all_nonpositive(lp_solution_, 0)) return exact_lp_candidate_result::negative_witness;
            bool preserves_required_rows = true;
            for (const size_t row : required_outside_rows) {
                if (lp_product_[lp_outside_rows_[row]].sign() < 0) {
                    preserves_required_rows = false;
                    break;
                }
            }
            if (!preserves_required_rows) continue;
            const coverage_score candidate_score = score(lp_solution_, 0, lp_product_);
            if (!better_score(candidate_score, selected_score)) continue;
            selected_score = candidate_score;
            selected_solution_ = lp_solution_;
            selected_product_ = lp_product_;
            selected = true;
        }
        if (!selected) return exact_lp_candidate_result::unchanged;
        solution_ = selected_solution_;
        product_ = selected_product_;
        current_score_ = selected_score;
        remove_common_content();
        return exact_lp_candidate_result::improved;
    }

    static void multiply_directions(const matrix_integer& directions, const matrix_integer& products,
                                    const std::vector<integer>& coefficients, matrix_integer& solution,
                                    std::vector<integer>& product)
    {
        for (size_t row = 0; row < directions.rows(); ++row) {
            solution(row, 0).set_zero();
            for (size_t column = 0; column < directions.cols(); ++column)
                solution(row, 0).addmul(directions(row, column), coefficients[column]);
        }
        for (size_t row = 0; row < products.rows(); ++row) {
            product[row].set_zero();
            for (size_t column = 0; column < products.cols(); ++column)
                product[row].addmul(products(row, column), coefficients[column]);
        }
    }

    static bool lp_problem_fits(size_t support_dimension, size_t outside_dimension) noexcept
    {
        constexpr size_t maximum_scaled_entries = size_t{8} * 1024 * 1024;
        return support_dimension == 0 || outside_dimension <= maximum_scaled_entries / support_dimension;
    }

    void remove_common_content()
    {
        integer content;
        integer next;
        for (size_t row = 0; row < solution_.rows(); ++row) {
            fmpz_gcd(next.native_handle(), content.native_handle(), solution_(row, 0).native_handle());
            content = next;
            if (content.is_one()) return;
        }
        if (content.is_zero() || content.is_one()) return;
        for (size_t row = 0; row < solution_.rows(); ++row) solution_(row, 0).divide_exact(content);
        for (integer& value : product_) value.divide_exact(content);
    }

    static bool all_nonpositive(const matrix_integer& vectors, size_t column)
    {
        bool result = true;
        for (size_t row = 0; row < vectors.rows(); ++row) result &= vectors(row, column).sign() <= 0;
        return result;
    }

    static coverage_score score(const matrix_integer& vectors, size_t vector_column, const std::vector<integer>& products)
    {
        size_t lower_size = 0;
        for (size_t row = 0; row < vectors.rows(); ++row) lower_size += !vectors(row, vector_column).is_zero();
        size_t upper_size = 0;
        for (const integer& value : products) upper_size += value.sign() >= 0;
        assert(upper_size >= lower_size);
        return {upper_size - lower_size, upper_size};
    }

    void calculate_product(
        const matrix_integer& matrix, const matrix_integer& vectors, size_t vector_column, std::vector<integer>& product)
    {
        for (integer& value : product) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices_.size(); ++local)
                product[row].addmul(matrix(row, indices_[local]), vectors(local, vector_column));
        }
    }

    void calculate_nonsingular_product(const matrix_integer& matrix, const matrix_integer& vectors, size_t vector_column,
                                       const integer& denominator, std::vector<integer>& product)
    {
        size_t local_row = 0;
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            product[row].set_zero();
            if (local_row < indices_.size() && row == indices_[local_row]) {
                product[row] = denominator;
                ++local_row;
                continue;
            }
            for (size_t local = 0; local < indices_.size(); ++local)
                product[row].addmul(matrix(row, indices_[local]), vectors(local, vector_column));
        }
    }

    void calculate_nonsingular_product(const matrix_integer& matrix, const matrix_integer& vectors, size_t vector_column,
                                       const integer& denominator, matrix_integer& products, size_t product_column)
    {
        size_t local_row = 0;
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            products(row, product_column).set_zero();
            if (local_row < indices_.size() && row == indices_[local_row]) {
                if (local_row == vector_column) products(row, product_column) = denominator;
                ++local_row;
                continue;
            }
            for (size_t local = 0; local < indices_.size(); ++local)
                products(row, product_column).addmul(matrix(row, indices_[local]), vectors(local, vector_column));
        }
    }

    bool add_certificate()
    {
        support lower(product_.size());
        support upper(product_.size());
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (!solution_(local, 0).is_zero()) {
                lower.set(indices_[local]);
                ++lower_size;
            }
        }
        for (size_t row = 0; row < product_.size(); ++row) {
            if (product_[row].sign() >= 0) {
                upper.set(row);
                ++upper_size;
            }
        }

#ifdef COPOSIT_SAT_A4_TESTING
        if (captured_lower_ != nullptr) {
            *captured_lower_ = lower;
            *captured_upper_ = upper;
            return true;
        }
#endif
        supports_->add_interval(lower, upper);
        if (diagnostics_.active()) diagnostics_.certificate(upper_size - lower_size, upper_size);
        return true;
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column)
                principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

#ifdef COPOSIT_SAT_A4_TESTING
    void reset_test_counters() noexcept
    {
        monotone_lp_attempt_count_ = 0;
        monotone_lp_feasible_count_ = 0;
        monotone_extension_count_ = 0;
        monotone_exact_rejection_count_ = 0;
        monotone_incumbent_upper_size_ = 0;
        monotone_final_upper_size_ = 0;
    }

    void publish_test_counters() const noexcept
    {
        last_monotone_lp_attempt_count = monotone_lp_attempt_count_;
        last_monotone_lp_feasible_count = monotone_lp_feasible_count_;
        last_monotone_extension_count = monotone_extension_count_;
        last_monotone_exact_rejection_count = monotone_exact_rejection_count_;
        last_monotone_incumbent_upper_size = monotone_incumbent_upper_size_;
        last_monotone_final_upper_size = monotone_final_upper_size_;
    }
#endif

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    matrix_integer directions_;
    matrix_integer direction_products_;
    matrix_integer lp_solution_;
    matrix_integer selected_solution_;
    std::vector<integer> product_;
    std::vector<integer> lp_product_;
    std::vector<integer> selected_product_;
    std::vector<integer> lp_coefficients_;
    std::vector<size_t> lp_outside_rows_;
    std::vector<size_t> lp_required_rows_;
    std::vector<std::vector<double>> lp_scaled_rows_;
    std::vector<size_t> indices_;
    coverage_score current_score_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    std::optional<interval_sat> supports_;
#ifdef COPOSIT_SAT_A4_TESTING
    size_t monotone_lp_attempt_count_ = 0;
    size_t monotone_lp_feasible_count_ = 0;
    size_t monotone_extension_count_ = 0;
    size_t monotone_exact_rejection_count_ = 0;
    size_t monotone_incumbent_upper_size_ = 0;
    size_t monotone_final_upper_size_ = 0;
    support* captured_lower_ = nullptr;
    support* captured_upper_ = nullptr;
#endif
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
    copositivity_classification result{true, true};
    if (!dickinson_checker(matrix.rows(), result).check(matrix)) result = {false, false};
    return result;
}

#ifdef COPOSIT_SAT_A4_TESTING
size_t sat_a4_monotone_lp_attempt_count_for_testing() noexcept { return last_monotone_lp_attempt_count; }
size_t sat_a4_monotone_lp_feasible_count_for_testing() noexcept { return last_monotone_lp_feasible_count; }
size_t sat_a4_monotone_extension_count_for_testing() noexcept { return last_monotone_extension_count; }
size_t sat_a4_monotone_exact_rejection_count_for_testing() noexcept { return last_monotone_exact_rejection_count; }
size_t sat_a4_monotone_incumbent_upper_size_for_testing() noexcept { return last_monotone_incumbent_upper_size; }
size_t sat_a4_monotone_final_upper_size_for_testing() noexcept { return last_monotone_final_upper_size; }
bool sat_a4_monotone_incumbent_contains_for_testing(size_t index) noexcept
{
    return index < last_monotone_incumbent_upper.size() && last_monotone_incumbent_upper[index];
}

bool sat_a4_certificate_for_testing(
    const matrix_integer& matrix, const std::vector<size_t>& indices, support& lower, support& upper)
{
    return dickinson_checker(matrix.rows(), copositivity_mode::copositive)
        .optimize_support_for_testing(matrix, indices, lower, upper);
}

size_t sat_a4_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_sat diagram(dimension);
    for (const auto& [lower_mask, upper_mask] : intervals) {
        support lower(dimension);
        support upper(dimension);
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
        }
        diagram.add_interval(lower, upper);
    }

    diagram.start_cardinality(cardinality);
    std::vector<size_t> indices;
    size_t count = 0;
    while (diagram.take_first(indices)) {
        support exact(dimension);
        for (const size_t index : indices) exact.set(index);
        diagram.add_interval(exact, exact);
        ++count;
    }
    return count;
}

size_t sat_a4_interval_count(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    interval_sat diagram(dimension);
    support lower(dimension);
    support upper(dimension);
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.interval_count();
}

size_t sat_a4_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    interval_sat diagram(dimension);
    support lower(dimension);
    support upper(dimension);
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) lower.set(bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) upper.set(bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.last_interval_clause_size();
}
#endif

} // namespace coposit::model
