#include <coposit/improved_nbc_upward_supports.hpp>
#include <coposit/timeout.hpp>

#include "third_party/improved_nbc_minisat_all/improved_nbc_api.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace coposit {

namespace {

struct interval {
    support lower;
    support upper;
    size_t lower_size;
    size_t upper_size;
};

bool contains(const interval& outer, const interval& inner) noexcept
{
    return outer.lower.is_subset_of(inner.lower) && inner.upper.is_subset_of(outer.upper);
}

} // namespace

class improved_nbc_upward_supports::impl {
public:
    explicit impl(size_t dimension)
        : dimension_(dimension)
    {
        if (dimension_ == 0 || dimension_ > static_cast<size_t>(std::numeric_limits<int>::max() - 1))
            throw std::invalid_argument("Improved NBC support dimension is outside its integer literal range");
        next_variable_ = static_cast<int>(dimension_) + 1;

        std::vector<int> wires;
        size_t padded_dimension = 1;
        while (padded_dimension < dimension_) {
            if (padded_dimension > static_cast<size_t>(std::numeric_limits<int>::max()) / 2)
                throw std::overflow_error("Improved NBC cardinality network is too large");
            padded_dimension *= 2;
        }
        wires.reserve(padded_dimension);
        for (size_t index = 0; index < dimension_; ++index) wires.push_back(variable(index));
        if (padded_dimension != dimension_) {
            const int constant_false = new_variable();
            base_clauses_.push_back({-constant_false});
            wires.resize(padded_dimension, constant_false);
        }
        bitonic_sort(wires, 0, wires.size(), true);
        cardinality_outputs_.assign(wires.begin(), wires.begin() + dimension_);
    }

    void add_interval(const support& lower, const support& upper)
    {
        if (lower.dimension() != dimension_ || upper.dimension() != dimension_ || !lower.is_subset_of(upper))
            throw std::invalid_argument("invalid Improved NBC support interval");
        pending_.push_back({lower, upper, lower.cardinality(), upper.cardinality()});
        if (lower.empty() && upper.cardinality() == dimension_) {
            all_future_covered_ = true;
            low_stream_.solver.reset();
            high_stream_.solver.reset();
            return;
        }
        const bool low_consistent = install_interval(low_stream_.solver.get(), pending_.back());
        const bool high_consistent = install_interval(high_stream_.solver.get(), pending_.back());
        if (!low_consistent || !high_consistent) {
            all_future_covered_ = true;
            low_stream_.solver.reset();
            high_stream_.solver.reset();
        }
    }

    void add_pair_upward_closure(size_t first, size_t second)
    {
        if (first >= dimension_ || second >= dimension_ || first == second)
            throw std::invalid_argument("invalid Improved NBC pair closure");
        support lower(dimension_);
        support ceiling(dimension_);
        lower.set(first);
        lower.set(second);
        ceiling.set_all();
        add_interval(lower, ceiling);
    }

    void add_upward_closure(const std::vector<size_t>& indices)
    {
        if (indices.empty()) throw std::invalid_argument("an upward closure needs a nonempty root");
        support lower(dimension_);
        support ceiling(dimension_);
        for (const size_t index : indices) {
            if (index >= dimension_) throw std::invalid_argument("Improved NBC upward root index is out of range");
            lower.set(index);
        }
        ceiling.set_all();
        add_interval(lower, ceiling);
    }

    void start_cardinality(size_t cardinality, bool high_frontier)
    {
        if (cardinality == 0 || cardinality > dimension_)
            throw std::invalid_argument("invalid Improved NBC cardinality stream");
        stream& selected = high_frontier ? high_stream_ : low_stream_;
        if (selected.cardinality == cardinality) return;
        selected.cardinality = cardinality;
        selected.exhausted = all_future_covered_;
        selected.unexplored.clear();
        if (selected.exhausted) {
            selected.solver.reset();
            return;
        }
        selected.unexplored.push_back({support(dimension_), 0});
        selected.solver = make_solver(true);
        selected.exhausted = selected.solver == nullptr;
    }

    bool take_first(std::vector<size_t>& indices, bool high_frontier)
    {
        stream& selected = high_frontier ? high_stream_ : low_stream_;
        if (selected.cardinality == 0)
            throw std::logic_error("Improved NBC cardinality stream was not started");
        if (selected.exhausted || all_future_covered_) return false;

        while (!selected.unexplored.empty()) {
            prefix current = std::move(selected.unexplored.back());
            selected.unexplored.pop_back();

            assumptions_.clear();
            assumptions_.push_back(cardinality_outputs_[selected.cardinality - 1]);
            if (selected.cardinality < dimension_) assumptions_.push_back(-cardinality_outputs_[selected.cardinality]);
            for (size_t index = 0; index < current.length; ++index)
                assumptions_.push_back(current.values.contains(index) ? variable(index) : -variable(index));

            indices.clear();
            visitor_ = &capture_first_support;
            visitor_state_ = &indices;
            callback_exception_ = nullptr;
            const int result = improved_nbc_solver_enumerate(selected.solver.get(), assumptions_.data(), static_cast<int>(assumptions_.size()),
                                                    &model_callback, this, &terminate_callback, nullptr);
            visitor_ = nullptr;
            visitor_state_ = nullptr;
            if (callback_exception_ != nullptr) std::rethrow_exception(callback_exception_);
            if (result == IMPROVED_NBC_ENUM_INTERRUPTED) {
                timeout_checkpoint();
                throw std::runtime_error("Improved NBC enumeration was interrupted without a coposit timeout");
            }
            if (result == IMPROVED_NBC_ENUM_ERROR) throw std::runtime_error("Improved NBC enumeration failed");
            if (result == IMPROVED_NBC_ENUM_EXHAUSTED) {
                if (improved_nbc_solver_is_inconsistent(selected.solver.get())) {
                    all_future_covered_ = true;
                    low_stream_.solver.reset();
                    high_stream_.solver.reset();
                    return false;
                }
                continue;
            }
            if (result != IMPROVED_NBC_ENUM_STOPPED) throw std::runtime_error("Improved NBC returned an unknown enumeration status");
            if (indices.size() != selected.cardinality)
                throw std::runtime_error("Improved NBC returned a support outside the requested cardinality");

            support model(dimension_);
            for (const size_t index : indices) model.set(index);
            for (size_t index = 0; index < current.length; ++index) {
                if (model.contains(index) != current.values.contains(index))
                    throw std::runtime_error("Improved NBC returned a support outside its unexplored prefix");
            }
            for (size_t index = current.length; index < dimension_; ++index) {
                support sibling = model;
                if (model.contains(index)) sibling.reset(index);
                else sibling.set(index);
                selected.unexplored.push_back({std::move(sibling), index + 1});
            }
            return true;
        }

        selected.exhausted = true;
        selected.solver.reset();
        return false;
    }

    enumeration_result enumerate_cardinality(size_t cardinality, void* state, visitor visit)
    {
        if (cardinality == 0 || cardinality > dimension_ || visit == nullptr)
            throw std::invalid_argument("invalid Improved NBC cardinality enumeration request");
        if (all_future_covered_) return enumeration_result::exhausted;

        solver_ptr solver = make_solver(false);
        if (!solver) return enumeration_result::exhausted;

        std::vector<int> assumptions{cardinality_outputs_[cardinality - 1]};
        if (cardinality < dimension_) assumptions.push_back(-cardinality_outputs_[cardinality]);
        visitor_ = visit;
        visitor_state_ = state;
        callback_exception_ = nullptr;
        const int result = improved_nbc_solver_enumerate(solver.get(), assumptions.data(), static_cast<int>(assumptions.size()),
                                                &model_callback, this, &terminate_callback, nullptr);
        visitor_ = nullptr;
        visitor_state_ = nullptr;
        if (callback_exception_ != nullptr) std::rethrow_exception(callback_exception_);
        if (result == IMPROVED_NBC_ENUM_INTERRUPTED) {
            timeout_checkpoint();
            throw std::runtime_error("Improved NBC enumeration was interrupted without a coposit timeout");
        }
        if (result == IMPROVED_NBC_ENUM_ERROR) throw std::runtime_error("Improved NBC enumeration failed");
        if (result == IMPROVED_NBC_ENUM_EXHAUSTED && improved_nbc_solver_is_inconsistent(solver.get())) all_future_covered_ = true;
        return result == IMPROVED_NBC_ENUM_STOPPED ? enumeration_result::stopped : enumeration_result::exhausted;
    }

    void commit_layer(size_t completed_cardinality)
    {
        commit_frontiers(completed_cardinality + 1, dimension_);
    }

    void commit_frontiers(size_t first_remaining_cardinality, size_t last_remaining_cardinality)
    {
        active_.insert(active_.end(), std::make_move_iterator(pending_.begin()), std::make_move_iterator(pending_.end()));
        pending_.clear();
        compact_full_upward_roots(first_remaining_cardinality == 0 ? 0 : first_remaining_cardinality - 1);
        if (all_future_covered_) {
            low_stream_.solver.reset();
            high_stream_.solver.reset();
            return;
        }
        compact_contained_intervals(first_remaining_cardinality, last_remaining_cardinality);
        rebuild_stream(low_stream_);
        rebuild_stream(high_stream_);
    }

    size_t interval_count() const noexcept { return active_.size() + pending_.size(); }
    bool all_future_covered() const noexcept { return all_future_covered_; }

private:
    struct solver_deleter {
        void operator()(improved_nbc_solver* solver) const noexcept { improved_nbc_solver_delete(solver); }
    };

    using solver_ptr = std::unique_ptr<improved_nbc_solver, solver_deleter>;

    struct prefix {
        support values;
        size_t length;
    };

    struct stream {
        size_t cardinality = 0;
        bool exhausted = false;
        std::vector<prefix> unexplored;
        solver_ptr solver;
    };

    static bool capture_first_support(void* opaque, const std::vector<size_t>& indices)
    {
        *static_cast<std::vector<size_t>*>(opaque) = indices;
        return false;
    }

    solver_ptr make_solver(bool include_pending)
    {
        solver_ptr solver(improved_nbc_solver_new());
        if (!solver) throw std::bad_alloc();
        improved_nbc_solver_set_variable_count(solver.get(), next_variable_ - 1);
        for (const auto& clause : base_clauses_) {
            const int result = improved_nbc_solver_add_clause(solver.get(), clause.data(), static_cast<int>(clause.size()));
            if (result < 0) throw std::runtime_error("Improved NBC rejected an internal cardinality clause");
            if (result == 0) {
                all_future_covered_ = true;
                return {};
            }
        }
        for (const interval& certificate : active_) {
            if (!install_interval(solver.get(), certificate)) {
                all_future_covered_ = true;
                return {};
            }
        }
        if (include_pending) {
            for (const interval& certificate : pending_) {
                if (!install_interval(solver.get(), certificate)) {
                    all_future_covered_ = true;
                    return {};
                }
            }
        }
        return solver;
    }

    bool install_interval(improved_nbc_solver* solver, const interval& certificate)
    {
        if (solver == nullptr) return true;
        clause_.clear();
        for (size_t index = 0; index < dimension_; ++index) {
            if (certificate.lower.contains(index)) clause_.push_back(-variable(index));
            else if (!certificate.upper.contains(index)) clause_.push_back(variable(index));
        }
        if (certificate.upper_size < dimension_) clause_.push_back(cardinality_outputs_[certificate.upper_size]);
        if (clause_.empty()) return false;
        const int result = improved_nbc_solver_add_clause(solver, clause_.data(), static_cast<int>(clause_.size()));
        if (result < 0) throw std::runtime_error("Improved NBC rejected a retained support certificate");
        return result != 0;
    }

    void rebuild_stream(stream& selected)
    {
        if (selected.cardinality == 0 || selected.exhausted) return;
        selected.solver = make_solver(false);
        selected.exhausted = selected.solver == nullptr;
    }

    static int model_callback(void* opaque, const signed char* assignments, int variable_count) noexcept
    {
        auto& self = *static_cast<impl*>(opaque);
        try {
            if (variable_count < self.next_variable_ - 1)
                throw std::runtime_error("Improved NBC returned an incomplete cardinality-network assignment");
            self.indices_.clear();
            for (size_t index = 0; index < self.dimension_; ++index)
                if (assignments[index] > 0) self.indices_.push_back(index);
            if (!self.visitor_(self.visitor_state_, self.indices_)) return 0;
            return 1;
        } catch (...) {
            self.callback_exception_ = std::current_exception();
            return 0;
        }
    }

    static int terminate_callback(void*) noexcept { return timeout_pending() ? 1 : 0; }

    int variable(size_t index) const noexcept { return static_cast<int>(index) + 1; }

    int new_variable()
    {
        if (next_variable_ == std::numeric_limits<int>::max())
            throw std::overflow_error("Improved NBC cardinality network exceeds its integer literal range");
        if ((next_variable_ & 4095) == 0) timeout_checkpoint();
        return next_variable_++;
    }

    std::pair<int, int> comparator(int first, int second)
    {
        const int high = new_variable();
        const int low = new_variable();
        base_clauses_.push_back({-first, high});
        base_clauses_.push_back({-second, high});
        base_clauses_.push_back({first, second, -high});
        base_clauses_.push_back({first, -low});
        base_clauses_.push_back({second, -low});
        base_clauses_.push_back({-first, -second, low});
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
        for (size_t index = first; index < first + half; ++index) compare_exchange(wires, index, index + half, descending);
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

    void compact_full_upward_roots(size_t completed_cardinality)
    {
        std::vector<support> roots;
        std::vector<interval> bounded;
        support ceiling(dimension_);
        ceiling.set_all();
        for (interval& certificate : active_) {
            if (certificate.upper == ceiling) roots.push_back(std::move(certificate.lower));
            else bounded.push_back(std::move(certificate));
        }

        if (std::any_of(roots.begin(), roots.end(), [](const support& root) { return root.empty(); })) {
            all_future_covered_ = true;
            active_.clear();
            return;
        }

        bool changed = true;
        std::vector<size_t> bits;
        while (changed && !roots.empty()) {
            changed = false;
            std::sort(roots.begin(), roots.end(), [](const support& left, const support& right) {
                const size_t left_size = left.cardinality();
                const size_t right_size = right.cardinality();
                return left_size != right_size ? left_size < right_size : left < right;
            });
            std::vector<support> minimal;
            std::vector<std::vector<size_t>> minimal_by_bit(dimension_);
            for (support& candidate : roots) {
                bool covered = false;
                candidate.copy_indices_to(bits);
                for (const size_t bit : bits) {
                    for (const size_t retained : minimal_by_bit[bit]) {
                        if (minimal[retained].is_subset_of(candidate)) {
                            covered = true;
                            break;
                        }
                    }
                    if (covered) break;
                }
                if (covered) continue;
                const size_t retained = minimal.size();
                minimal.push_back(std::move(candidate));
                minimal_by_bit[minimal[retained].lowest_index()].push_back(retained);
            }
            roots.swap(minimal);

            std::map<support, size_t> parent_counts;
            for (const support& root : roots) {
                if (root.cardinality() == 0 || root.cardinality() > completed_cardinality) continue;
                support parent = root;
                root.copy_indices_to(bits);
                for (const size_t bit : bits) {
                    parent.reset(bit);
                    ++parent_counts[parent];
                    parent.set(bit);
                }
            }
            for (const auto& [parent, count] : parent_counts) {
                if (count != dimension_ - parent.cardinality()) continue;
                if (parent.empty()) {
                    all_future_covered_ = true;
                    active_.clear();
                    return;
                }
                roots.push_back(parent);
                changed = true;
            }
        }

        active_ = std::move(bounded);
        for (support& root : roots) active_.push_back({std::move(root), ceiling, 0, dimension_});
        for (interval& certificate : active_) certificate.lower_size = certificate.lower.cardinality();
    }

    void compact_contained_intervals(size_t first_remaining_cardinality, size_t last_remaining_cardinality)
    {
        active_.erase(std::remove_if(active_.begin(), active_.end(), [&](const interval& certificate) {
            return certificate.upper_size < first_remaining_cardinality || certificate.lower_size > last_remaining_cardinality;
        }), active_.end());
        if (active_.size() < 2) return;
        std::sort(active_.begin(), active_.end(), [](const interval& left, const interval& right) {
            if (left.lower_size != right.lower_size) return left.lower_size < right.lower_size;
            if (left.upper_size != right.upper_size) return left.upper_size > right.upper_size;
            if (left.lower != right.lower) return left.lower < right.lower;
            return left.upper < right.upper;
        });

        std::vector<interval> compacted;
        std::vector<std::vector<size_t>> by_lower_bit(dimension_ + 1);
        std::vector<size_t> bits;
        compacted.reserve(active_.size());
        for (interval& candidate : active_) {
            bool covered = false;
            for (const size_t retained_index : by_lower_bit[dimension_]) {
                const interval& retained = compacted[retained_index];
                if (retained.upper_size >= candidate.upper_size && contains(retained, candidate)) {
                    covered = true;
                    break;
                }
            }
            candidate.lower.copy_indices_to(bits);
            for (const size_t bit : bits) {
                if (covered) break;
                for (const size_t retained_index : by_lower_bit[bit]) {
                    const interval& retained = compacted[retained_index];
                    if (retained.lower_size <= candidate.lower_size && retained.upper_size >= candidate.upper_size
                        && contains(retained, candidate)) {
                        covered = true;
                        break;
                    }
                }
                if (covered) break;
            }
            if (covered) continue;
            const size_t index = compacted.size();
            compacted.push_back(std::move(candidate));
            const size_t bucket = compacted[index].lower.empty() ? dimension_ : compacted[index].lower.lowest_index();
            by_lower_bit[bucket].push_back(index);
        }
        active_.swap(compacted);
    }


    size_t dimension_;
    int next_variable_ = 1;
    std::vector<std::vector<int>> base_clauses_;
    std::vector<int> cardinality_outputs_;
    std::vector<interval> active_;
    std::vector<interval> pending_;
    stream low_stream_;
    stream high_stream_;
    std::vector<int> clause_;
    std::vector<int> assumptions_;
    std::vector<size_t> indices_;
    visitor visitor_ = nullptr;
    void* visitor_state_ = nullptr;
    std::exception_ptr callback_exception_;
    bool all_future_covered_ = false;
};

improved_nbc_upward_supports::improved_nbc_upward_supports(size_t dimension)
    : impl_(std::make_unique<impl>(dimension))
{
}

improved_nbc_upward_supports::~improved_nbc_upward_supports() = default;

void improved_nbc_upward_supports::add_interval(const support& lower, const support& upper) { impl_->add_interval(lower, upper); }
void improved_nbc_upward_supports::add_pair_upward_closure(size_t first, size_t second) { impl_->add_pair_upward_closure(first, second); }
void improved_nbc_upward_supports::add_upward_closure(const std::vector<size_t>& indices) { impl_->add_upward_closure(indices); }

void improved_nbc_upward_supports::start_cardinality(size_t cardinality, bool high_frontier)
{
    impl_->start_cardinality(cardinality, high_frontier);
}

bool improved_nbc_upward_supports::take_first(std::vector<size_t>& indices, bool high_frontier)
{
    return impl_->take_first(indices, high_frontier);
}

improved_nbc_upward_supports::enumeration_result improved_nbc_upward_supports::enumerate_cardinality(
    size_t cardinality, void* state, visitor visit)
{
    return impl_->enumerate_cardinality(cardinality, state, visit);
}

void improved_nbc_upward_supports::commit_layer(size_t completed_cardinality) { impl_->commit_layer(completed_cardinality); }
void improved_nbc_upward_supports::commit_frontiers(size_t first_remaining_cardinality, size_t last_remaining_cardinality)
{
    impl_->commit_frontiers(first_remaining_cardinality, last_remaining_cardinality);
}
size_t improved_nbc_upward_supports::interval_count() const noexcept { return impl_->interval_count(); }
bool improved_nbc_upward_supports::all_future_covered() const noexcept { return impl_->all_future_covered(); }

} // namespace coposit
