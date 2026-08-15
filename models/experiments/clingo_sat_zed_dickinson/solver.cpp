#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/progress.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <clingo.hh>

#include "source_trace.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace coposit::model {

namespace {

constexpr char selection_program[] = "{ selected(0..n-1) }.";
constexpr char cardinality_program[] = R"(
#external active(k).
:- active(k), #count { Index : selected(Index) } != k.
)";

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

bool has_motzkin_straus_pattern(const matrix_integer& matrix)
{
    const integer::const_reference nonedge = matrix(0, 0);
    if (nonedge.sign() < 0) return false;

    integer edge;
    bool has_edge = false;
    for (size_t row = 0; row < matrix.rows(); ++row) {
        timeout_checkpoint();
        if (matrix(row, row).compare(nonedge) != 0) return false;
        for (size_t column = row + 1; column < matrix.rows(); ++column) {
            const integer::const_reference value = matrix(row, column);
            if (value.compare(nonedge) == 0) continue;
            if (value.sign() >= 0) return false;
            if (!has_edge) {
                edge = value;
                has_edge = true;
            } else if (value.compare(edge) != 0) {
                return false;
            }
        }
    }
    return has_edge;
}

bool configured_zed_scan()
{
    const char* text = std::getenv("COPOSIT_CLINGO_SAT_ZED_SCAN");
    if (text == nullptr || std::string_view(text) == "on") return true;
    if (std::string_view(text) == "off") return false;
    throw std::invalid_argument("COPOSIT_CLINGO_SAT_ZED_SCAN must be 'on' or 'off'");
}

class dickinson_checker final : public Clingo::SolveEventHandler {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(mode)
        , progress_(progress::metric::support, dimension)
    {
        indices_.reserve(dimension);
        selected_literals_.reserve(dimension);
        clause_.reserve(dimension);
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : factorization_(dimension)
        , product_(dimension)
        , mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , progress_(progress::metric::support, dimension)
    {
        indices_.reserve(dimension);
        selected_literals_.reserve(dimension);
        clause_.reserve(dimension);
    }

    bool check(const matrix_integer& matrix)
    {
        if (matrix.rows() > static_cast<size_t>(std::numeric_limits<int>::max()))
            throw std::overflow_error("clingo supports at most INT_MAX matrix indices");

        matrix_ = &matrix;
        const std::array<char const*, 4> arguments{"--parallel-mode=1", "--enum-mode=bt", "--models=0", "--warn=none"};
        Clingo::Control control(arguments);
        control.enable_enumeration_assumption(false);
        control.add("base", {"n"}, selection_program);
        control.add("cardinality", {"k"}, cardinality_program);
        const Clingo::Symbol dimension = Clingo::Number(static_cast<int>(matrix.rows()));
        control.ground({Clingo::Part{"base", {dimension}}});
        const Clingo::SymbolicAtoms atoms = control.symbolic_atoms();
        for (size_t index = 0; index < matrix.rows(); ++index) {
            const Clingo::Symbol atom = Clingo::Function("selected", {Clingo::Number(static_cast<int>(index))});
            auto iterator = atoms.find(atom);
            if (!iterator) throw std::runtime_error("clingo did not ground a selected/1 support atom");
            selected_literals_.push_back(iterator->literal());
        }

        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            timeout_checkpoint();
            progress_.stage(subset_dimension);
            current_cardinality_ = subset_dimension;
            const Clingo::Symbol cardinality = Clingo::Number(static_cast<int>(subset_dimension));
            control.ground({Clingo::Part{"cardinality", {cardinality}}});
            const Clingo::Symbol active = Clingo::Function("active", {cardinality});
            auto active_atom = control.symbolic_atoms().find(active);
            if (!active_atom) throw std::runtime_error("clingo did not ground the active cardinality atom");
            const Clingo::literal_t active_literal = active_atom->literal();
            control.assign_external(active_literal, Clingo::TruthValue::True);

            auto handle = control.solve(Clingo::SymbolicLiteralSpan{}, this, true, false);
#ifdef COPOSIT_ENABLE_TIMEOUTS
            while (!handle.wait(0.05)) {
                if (!timeout_pending()) continue;
                timed_out_ = true;
                handle.cancel();
                break;
            }
#else
            handle.wait();
#endif
            const Clingo::SolveResult result = handle.get();
            handle.close();
            control.assign_external(active_literal, Clingo::TruthValue::False);

            if (timed_out_) throw timeout_requested{};
            if (failed_) {
                progress_.finish();
                return false;
            }
            if (result.is_interrupted()) {
                timeout_checkpoint();
                throw std::runtime_error("clasp interrupted without a coposit timeout");
            }
            if (!result.is_exhausted()) throw std::runtime_error("clasp stopped before exhausting the active cardinality");
        }

        progress_.finish();
        return true;
    }

private:
    bool on_model(Clingo::Model& model) override
    {
        timeout_checkpoint();
        indices_.clear();
        for (size_t index = 0; index < selected_literals_.size(); ++index)
            if (model.is_true(selected_literals_[index])) indices_.push_back(index);
        if (indices_.size() != current_cardinality_)
            throw std::runtime_error("clasp produced a completed support outside the active cardinality");

        progress_.visit_support();
        progress_.secondary();
        COPOSIT_CLINGO_SAT_ZED_TRACE("process", current_cardinality_);
        Clingo::SolveControl control = model.context();
        if (!process_subset(*matrix_, control)) {
            failed_ = true;
            control.add_clause(Clingo::LiteralSpan{});
            return false;
        }
        return true;
    }

    bool process_z_block(const matrix_integer& matrix, const std::vector<size_t>& indices)
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
            COPOSIT_CLINGO_SAT_ZED_TRACE("z-component", component.size());
            const bool positive_semidefinite = factorization_.is_positive_semidefinite();
            const bool positive_definite = positive_semidefinite && factorization_.is_positive_definite();
            if (classification_ != nullptr) {
                if (!positive_semidefinite) {
                    COPOSIT_CLINGO_SAT_ZED_TRACE("z-reject", indices.size());
                    return false;
                }
                if (!positive_definite) classification_->is_strictly_copositive = false;
            } else if (!(mode_ == copositivity_mode::strictly_copositive ? positive_definite : positive_semidefinite)) {
                COPOSIT_CLINGO_SAT_ZED_TRACE("z-reject", indices.size());
                return false;
            }
        }

        COPOSIT_CLINGO_SAT_ZED_TRACE("z-pass", indices.size());
        return true;
    }

    bool process_subset(const matrix_integer& matrix, Clingo::SolveControl& control)
    {
        const size_t dimension = indices_.size();
        principal_.resize(dimension, dimension);
        solution_.resize(dimension, 1);
        copy_principal(matrix, indices_, principal_);

        const bool singular = factorization_.factorize_inplace(principal_) == 0;
        if (!singular) {
            for (size_t row = 0; row < dimension; ++row) solution_(row, 0).set_one();

            integer denominator;
            factorization_.solve_inplace(solution_, denominator, principal_);
            assert(denominator.sign() > 0);
        } else {
            factorization_.one_nullspace_vector(solution_, principal_);

            bool has_positive_entry = false;
            for (size_t row = 0; row < dimension; ++row) has_positive_entry |= solution_(row, 0).sign() > 0;
            if (!has_positive_entry) solution_.negate();
        }

        bool all_nonpositive = true;
        bool all_nonnegative = singular;
        for (size_t row = 0; row < dimension; ++row) {
            all_nonpositive &= solution_(row, 0).sign() <= 0;
            all_nonnegative &= solution_(row, 0).sign() >= 0;
        }
        if (all_nonpositive) return false;
        if (all_nonnegative) {
            if (classification_ != nullptr) classification_->is_strictly_copositive = false;
            else if (mode_ == copositivity_mode::strictly_copositive) return false;
        }

        if (progress_.active()) {
            const auto [free_indices, upper_size] = add_certificate<true>(matrix, control);
            progress_.certificate(free_indices, upper_size);
        } else {
            add_certificate<false>(matrix, control);
        }
        return true;
    }

    template <bool CountSizes>
    std::pair<size_t, size_t> add_certificate(const matrix_integer& matrix, Clingo::SolveControl& control)
    {
        clause_.clear();
        size_t lower_size = 0;
        size_t upper_size = 0;
        for (size_t local = 0; local < indices_.size(); ++local) {
            if (!solution_(local, 0).is_zero()) {
                clause_.push_back(-selected_literals_[indices_[local]]);
                if constexpr (CountSizes) ++lower_size;
            }
        }

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices_.size(); ++local)
                product_[row].addmul(matrix(row, indices_[local]), solution_(local, 0));
            if (product_[row].sign() >= 0) {
                if constexpr (CountSizes) ++upper_size;
            } else {
                clause_.push_back(selected_literals_[row]);
            }
        }

        control.add_clause(Clingo::LiteralSpan{clause_});
        if constexpr (CountSizes) return {upper_size - lower_size, upper_size};
        return {0, 0};
    }

    static void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
    {
        for (size_t row = 0; row < indices.size(); ++row) {
            timeout_checkpoint();
            for (size_t column = 0; column <= row; ++column)
                principal(row, column) = matrix(indices[row], indices[column]);
        }
    }

    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    std::vector<integer> product_;
    std::vector<size_t> indices_;
    std::vector<Clingo::literal_t> selected_literals_;
    std::vector<Clingo::literal_t> clause_;
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    progress::tracker progress_;
    const matrix_integer* matrix_ = nullptr;
    size_t current_cardinality_ = 0;
    bool failed_ = false;
    bool timed_out_ = false;
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

} // namespace coposit::model
