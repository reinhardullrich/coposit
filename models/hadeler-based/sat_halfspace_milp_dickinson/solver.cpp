#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/milp_solver.hpp>
#include <coposit/model.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cadical.hpp>

#include "source_diagnostics.hpp"

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

bool negative_orientation_has_larger_upper(size_t positive_products, size_t negative_products) noexcept
{
    return negative_products > positive_products;
}

#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
size_t last_milp_improvement_count = 0;
#endif

class timeout_terminator final : public CaDiCaL::Terminator {
public:
    bool terminate() override { return timeout_pending(); }
};

class interval_sat {
public:
    explicit interval_sat(const support_context& context)
        : context_(context)
        , dimension_(context.dimension())
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
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
        last_interval_clause_size_ = 0;
#endif
        for (size_t index = 0; index < dimension_; ++index) {
            const bool in_upper = context_.contains(upper, index);
            if (in_upper) ++upper_size;
            if (context_.contains(lower, index)) {
                solver_.add(-variable(index));
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
                ++last_interval_clause_size_;
#endif
            } else if (!in_upper) {
                solver_.add(variable(index));
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
                ++last_interval_clause_size_;
#endif
            }
        }
        if (upper_size < dimension_) {
            solver_.add(cardinality_outputs_[upper_size]);
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
            ++last_interval_clause_size_;
#endif
        }
        solver_.add(0);
        ++interval_count_;
    }

    size_t interval_count() const noexcept { return interval_count_; }
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
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

    const support_context& context_;


    size_t dimension_;
    int next_variable_ = 1;
    size_t cardinality_ = 0;
    size_t interval_count_ = 0;
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
    size_t last_interval_clause_size_ = 0;
#endif
    timeout_terminator terminator_;
    CaDiCaL::Solver solver_;
    std::vector<int> cardinality_outputs_;
};

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , mode_(mode)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : support_context_(dimension)
        , factorization_(dimension)
        , product_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , diagnostics_(diagnostics::metric::support, dimension)
    {
        indices_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
        milp_improvement_count_ = 0;
#endif
        supports_.emplace(support_context_);
        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            diagnostics_.stage(subset_dimension);
            supports_->start_cardinality(subset_dimension);
            while (supports_->take_first(indices_)) {
                timeout_checkpoint();
                diagnostics_.visit_support();
                diagnostics_.secondary();
                COPOSIT_SAT_HALFSPACE_MILP_DIAGNOSTICS("process", subset_dimension);
                if (!process_subset(matrix)) {
                    diagnostics_.finish();
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
                    last_milp_improvement_count = milp_improvement_count_;
#endif
                    return false;
                }
            }
        }

        diagnostics_.finish();
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
        last_milp_improvement_count = milp_improvement_count_;
#endif
        return true;
    }

private:
    bool process_subset(const matrix_integer& matrix)
    {
        const size_t dimension = indices_.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices_, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
        if (singular && diagnostics_.active()) diagnostics_.singular_support(dimension - factorization_.rank());
        return singular ? process_singular_subset(matrix) : process_nonsingular_subset(matrix);
    }

    bool process_singular_subset(const matrix_integer& matrix)
    {
        factorization_.one_nullspace_vector(solution_, principal_);

        bool has_positive_entry = false;
        bool has_negative_entry = false;
        for (size_t local = 0; local < indices_.size(); ++local) {
            has_positive_entry |= solution_(local, 0).sign() > 0;
            has_negative_entry |= solution_(local, 0).sign() < 0;
        }
        assert(has_positive_entry || has_negative_entry);
        if (!has_positive_entry) {
            solution_.negate();
            has_negative_entry = false;
        }

        if (!has_negative_entry) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

        calculate_product(matrix, solution_, product_);
        if (has_negative_entry) {
            size_t positive_products = 0;
            size_t negative_products = 0;
            for (const integer& value : product_) {
                positive_products += value.sign() > 0;
                negative_products += value.sign() < 0;
            }
            if (negative_orientation_has_larger_upper(positive_products, negative_products)) {
                solution_.negate();
                for (integer& value : product_) value.negate();
            }
        }
        return add_certificate();
    }

    bool process_nonsingular_subset(const matrix_integer& matrix)
    {
        const size_t dimension = indices_.size();
        for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

        integer denominator;
        factorization_.solve_inplace(solution_, denominator, principal_);
        assert(denominator.sign() > 0);
        if (all_nonpositive(solution_)) return false;

        calculate_nonsingular_product(matrix, solution_, 0, denominator, product_);
        current_upper_size_ = count_nonnegative(product_);
        if (dimension > 1 && current_upper_size_ < matrix.rows() && milp_problem_fits(dimension, matrix.rows())) {
            directions_.resize(dimension, dimension);
            for (size_t row = 0; row < dimension; ++row)
                for (size_t column = 0; column < dimension; ++column) {
                    if (row == column) directions_(row, column).set_one();
                    else directions_(row, column).set_zero();
                }

            integer direction_denominator;
            factorization_.solve_inplace(directions_, direction_denominator, principal_);
            assert(direction_denominator.compare(denominator) == 0);
            direction_products_.resize(matrix.rows(), dimension);
            for (size_t direction = 0; direction < dimension; ++direction)
                calculate_nonsingular_product(
                    matrix, directions_, direction, direction_denominator, direction_products_, direction);

            if (!optimize_with_milp()) return false;
        }
        return add_certificate();
    }

    bool optimize_with_milp()
    {
        timeout_checkpoint();
        const size_t support_dimension = indices_.size();
        const size_t outside_dimension = product_.size() - support_dimension;
        if (outside_dimension == 0) return true;

        std::vector<size_t> outside_rows;
        outside_rows.reserve(outside_dimension);
        size_t local_row = 0;
        for (size_t row = 0; row < product_.size(); ++row) {
            if (local_row < indices_.size() && row == indices_[local_row]) ++local_row;
            else outside_rows.push_back(row);
        }

        std::vector<std::vector<double>> scaled_rows(outside_dimension, std::vector<double>(support_dimension));
        for (size_t outside = 0; outside < outside_dimension; ++outside) {
            const size_t row = outside_rows[outside];
            slong maximum_exponent = std::numeric_limits<slong>::min();
            for (size_t column = 0; column < support_dimension; ++column) {
                if (direction_products_(row, column).is_zero()) continue;
                slong exponent = 0;
                static_cast<void>(direction_products_(row, column).to_dbl_2exp(exponent));
                maximum_exponent = std::max(maximum_exponent, exponent);
            }
            if (maximum_exponent == std::numeric_limits<slong>::min()) continue;
            for (size_t column = 0; column < support_dimension; ++column) {
                if (direction_products_(row, column).is_zero()) continue;
                slong exponent = 0;
                const double mantissa = direction_products_(row, column).to_dbl_2exp(exponent);
                const slong difference = exponent - maximum_exponent;
                scaled_rows[outside][column] =
                    difference < -1074 ? 0.0 : std::scalbn(mantissa, static_cast<int>(difference));
            }
        }

        const double positive_floor = std::min(1e-7, 0.5 / static_cast<double>(support_dimension));
        maximum_halfspaces_milp_solver optimizer(
            scaled_rows, positive_floor, current_upper_size_ - support_dimension, 10000,
            std::chrono::steady_clock::now() + std::chrono::milliseconds(20));
        auto result = optimizer.solve();
        timeout_checkpoint();
        if (result.point.empty()) return true;

        milp_coefficients_.resize(support_dimension);
        milp_solution_.resize(support_dimension, 1);
        milp_product_.resize(product_.size());
        size_t best_upper_size = current_upper_size_;
        bool improved = false;
        for (const uint64_t scale :
             {uint64_t{1000000}, uint64_t{1000000000}, uint64_t{1000000000000}, uint64_t{1000000000000000}}) {
            bool positive = true;
            for (size_t column = 0; column < support_dimension; ++column) {
                const double value = result.point[column];
                if (!(value > 0.0) || !std::isfinite(value) ||
                    value > static_cast<double>(std::numeric_limits<uint64_t>::max()) / static_cast<double>(scale)) {
                    positive = false;
                    break;
                }
                const uint64_t coefficient = static_cast<uint64_t>(std::llround(value * static_cast<double>(scale)));
                if (coefficient == 0) {
                    positive = false;
                    break;
                }
                fmpz_set_ui(milp_coefficients_[column].native_handle(), static_cast<ulong>(coefficient));
            }
            if (!positive) continue;

            multiply_directions(directions_, direction_products_, milp_coefficients_, milp_solution_, milp_product_);
            if (all_nonpositive(milp_solution_)) return false;
            const size_t upper_size = count_nonnegative(milp_product_);
            if (upper_size <= best_upper_size) continue;
            best_upper_size = upper_size;
            best_solution_ = milp_solution_;
            best_product_ = milp_product_;
            improved = true;
        }

        if (!improved) return true;
        solution_ = best_solution_;
        product_ = best_product_;
        current_upper_size_ = best_upper_size;
        remove_common_content();
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
        ++milp_improvement_count_;
#endif
        return true;
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

    static bool all_nonpositive(const matrix_integer& vector)
    {
        bool result = true;
        for (size_t row = 0; row < vector.rows(); ++row) result &= vector(row, 0).sign() <= 0;
        return result;
    }

    static bool milp_problem_fits(size_t support_dimension, size_t matrix_dimension) noexcept
    {
        // ponytail: cap the optional exact inverse/product workspace and scaled input; stream rows if this cap is ever the
        // measured reason a useful large support misses MILP optimization.
        constexpr size_t maximum_direction_entries = size_t{8} * 1024 * 1024;
        if (support_dimension > maximum_direction_entries || matrix_dimension > maximum_direction_entries - support_dimension)
            return false;
        return support_dimension <= maximum_direction_entries / (matrix_dimension + support_dimension);
    }

    static size_t count_nonnegative(const std::vector<integer>& values)
    {
        size_t result = 0;
        for (const integer& value : values) result += value.sign() >= 0;
        return result;
    }

    void calculate_product(const matrix_integer& matrix, const matrix_integer& vector, std::vector<integer>& product)
    {
        for (integer& value : product) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices_.size(); ++local)
                product[row].addmul(matrix(row, indices_[local]), vector(local, 0));
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

    bool add_certificate()
    {
        support lower = support_context_.make();
        support upper = support_context_.make();
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (!solution_(local, 0).is_zero()) {
                support_context_.set(lower, indices_[local]);
                ++lower_size;
            }
        }
        for (size_t row = 0; row < product_.size(); ++row) {
            if (product_[row].sign() >= 0) {
                support_context_.set(upper, row);
                ++upper_size;
            }
        }

        bool solution_nonnegative = true;
        integer quadratic;
        for (size_t local = 0; local < indices_.size(); ++local) {
            solution_nonnegative &= solution_(local, 0).sign() >= 0;
            quadratic.addmul(solution_(local, 0), product_[indices_[local]]);
        }
        if (solution_nonnegative && quadratic.is_zero()) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

        supports_->add_interval(lower, upper);
        if (diagnostics_.active()) diagnostics_.certificate(upper_size - lower_size, upper_size);
        support_context_.release(std::move(lower));
        support_context_.release(std::move(upper));
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

    support_context support_context_;

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    matrix_integer directions_;
    matrix_integer direction_products_;
    matrix_integer milp_solution_;
    matrix_integer best_solution_;
    std::vector<integer> product_;
    std::vector<integer> milp_coefficients_;
    std::vector<integer> milp_product_;
    std::vector<integer> best_product_;
    std::vector<size_t> indices_;
    size_t current_upper_size_ = 0;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    diagnostics::tracker diagnostics_;
    std::optional<interval_sat> supports_;
#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
    size_t milp_improvement_count_ = 0;
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

#ifdef COPOSIT_SAT_HALFSPACE_MILP_DICKINSON_TESTING
size_t sat_halfspace_milp_improvement_count_for_testing() noexcept
{
    return last_milp_improvement_count;
}

bool sat_halfspace_milp_prefers_negative_singular_orientation_for_testing(
    size_t positive_products, size_t negative_products) noexcept
{
    return negative_orientation_has_larger_upper(positive_products, negative_products);
}

size_t sat_halfspace_milp_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    support_context support_context_(dimension);
    interval_sat diagram(support_context_);
    for (const auto& [lower_mask, upper_mask] : intervals) {
        support lower = support_context_.make();
        support upper = support_context_.make();
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) support_context_.set(lower, bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) support_context_.set(upper, bit);
        }
        diagram.add_interval(lower, upper);
    }

    diagram.start_cardinality(cardinality);
    std::vector<size_t> indices;
    size_t count = 0;
    while (diagram.take_first(indices)) {
        support exact = support_context_.make();
        for (const size_t index : indices) support_context_.set(exact, index);
        diagram.add_interval(exact, exact);
        support_context_.release(std::move(exact));
        ++count;
    }
    return count;
}

size_t sat_halfspace_milp_interval_count(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    support_context support_context_(dimension);
    interval_sat diagram(support_context_);
    support lower = support_context_.make();
    support upper = support_context_.make();
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) support_context_.set(lower, bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) support_context_.set(upper, bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.interval_count();
}

size_t sat_halfspace_milp_interval_clause_size(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    support_context support_context_(dimension);
    interval_sat diagram(support_context_);
    support lower = support_context_.make();
    support upper = support_context_.make();
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) support_context_.set(lower, bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) support_context_.set(upper, bit);
    }
    diagram.add_interval(lower, upper);
    return diagram.last_interval_clause_size();
}
#endif

} // namespace coposit::model
