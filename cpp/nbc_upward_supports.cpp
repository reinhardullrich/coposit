#include <coposit/nbc_upward_supports.hpp>
#include <coposit/timeout.hpp>

#include "third_party/nbc_minisat_all/nbc_api.h"

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

bool contains(const support_context& context, const interval& outer, const interval& inner) noexcept
{
    return context.is_subset_of(outer.lower, inner.lower) && context.is_subset_of(inner.upper, outer.upper);
}

} // namespace

class nbc_upward_supports::impl {
public:
    explicit impl(support_context& context)
        : support_context_(context)
        , dimension_(context.dimension())
    {
        if (dimension_ == 0 || dimension_ > static_cast<size_t>(std::numeric_limits<int>::max() - 1))
            throw std::invalid_argument("NBC support dimension is outside its integer literal range");
        next_variable_ = static_cast<int>(dimension_) + 1;

        std::vector<int> wires;
        size_t padded_dimension = 1;
        while (padded_dimension < dimension_) {
            if (padded_dimension > static_cast<size_t>(std::numeric_limits<int>::max()) / 2)
                throw std::overflow_error("NBC cardinality network is too large");
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
        if (!support_context_.is_subset_of(lower, upper)) throw std::invalid_argument("invalid NBC support interval");
        pending_.push_back({support_context_.clone(lower), support_context_.clone(upper), support_context_.count(lower), support_context_.count(upper)});
        if (support_context_.empty(lower) && support_context_.count(upper) == dimension_) {
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
            throw std::invalid_argument("invalid NBC pair closure");
        support lower = support_context_.make();
        support ceiling = support_context_.make();
        support_context_.set(lower, first);
        support_context_.set(lower, second);
        support_context_.set_all(ceiling);
        add_interval(lower, ceiling);
        support_context_.release(std::move(lower));
        support_context_.release(std::move(ceiling));
    }

    void add_upward_closure(const std::vector<size_t>& indices)
    {
        if (indices.empty()) throw std::invalid_argument("an upward closure needs a nonempty root");
        support lower = support_context_.make();
        support ceiling = support_context_.make();
        for (const size_t index : indices) {
            if (index >= dimension_) throw std::invalid_argument("NBC upward root index is out of range");
            support_context_.set(lower, index);
        }
        support_context_.set_all(ceiling);
        add_interval(lower, ceiling);
        support_context_.release(std::move(lower));
        support_context_.release(std::move(ceiling));
    }

    void start_cardinality(size_t cardinality, bool high_frontier)
    {
        if (cardinality == 0 || cardinality > dimension_)
            throw std::invalid_argument("invalid NBC cardinality stream");
        stream& selected = high_frontier ? high_stream_ : low_stream_;
        if (selected.cardinality == cardinality) return;
        selected.cardinality = cardinality;
        selected.exhausted = all_future_covered_;
        clear_prefixes(selected.unexplored);
        if (selected.exhausted) {
            selected.solver.reset();
            return;
        }
        selected.unexplored.push_back({support_context_.make(), 0});
        selected.solver = make_solver(true);
        selected.exhausted = selected.solver == nullptr;
    }

    bool take_first(std::vector<size_t>& indices, bool high_frontier)
    {
        stream& selected = high_frontier ? high_stream_ : low_stream_;
        if (selected.cardinality == 0)
            throw std::logic_error("NBC cardinality stream was not started");
        if (selected.exhausted || all_future_covered_) return false;

        while (!selected.unexplored.empty()) {
            prefix current = std::move(selected.unexplored.back());
            selected.unexplored.pop_back();

            assumptions_.clear();
            assumptions_.push_back(cardinality_outputs_[selected.cardinality - 1]);
            if (selected.cardinality < dimension_) assumptions_.push_back(-cardinality_outputs_[selected.cardinality]);
            for (size_t index = 0; index < current.length; ++index)
                assumptions_.push_back(support_context_.contains(current.values, index) ? variable(index) : -variable(index));

            indices.clear();
            visitor_ = &capture_first_support;
            visitor_state_ = &indices;
            callback_exception_ = nullptr;
            const int result = nbc_solver_enumerate(selected.solver.get(), assumptions_.data(), static_cast<int>(assumptions_.size()),
                                                    &model_callback, this, &terminate_callback, nullptr);
            visitor_ = nullptr;
            visitor_state_ = nullptr;
            if (callback_exception_ != nullptr) std::rethrow_exception(callback_exception_);
            if (result == NBC_ENUM_INTERRUPTED) {
                timeout_checkpoint();
                throw std::runtime_error("NBC enumeration was interrupted without a coposit timeout");
            }
            if (result == NBC_ENUM_ERROR) throw std::runtime_error("NBC enumeration failed");
            if (result == NBC_ENUM_EXHAUSTED) {
                support_context_.release(std::move(current.values));
                continue;
            }
            if (result != NBC_ENUM_STOPPED) throw std::runtime_error("NBC returned an unknown enumeration status");
            if (indices.size() != selected.cardinality)
                throw std::runtime_error("NBC returned a support outside the requested cardinality");

            support model = support_context_.make();
            for (const size_t index : indices) support_context_.set(model, index);
            for (size_t index = 0; index < current.length; ++index) {
                if (support_context_.contains(model, index) != support_context_.contains(current.values, index))
                    throw std::runtime_error("NBC returned a support outside its unexplored prefix");
            }
            for (size_t index = current.length; index < dimension_; ++index) {
                support sibling = support_context_.clone(model);
                if (support_context_.contains(model, index)) support_context_.reset(sibling, index);
                else support_context_.set(sibling, index);
                selected.unexplored.push_back({std::move(sibling), index + 1});
            }
            support_context_.release(std::move(model));
            support_context_.release(std::move(current.values));
            return true;
        }

        selected.exhausted = true;
        selected.solver.reset();
        return false;
    }

    enumeration_result enumerate_cardinality(size_t cardinality, void* state, visitor visit)
    {
        if (cardinality == 0 || cardinality > dimension_ || visit == nullptr)
            throw std::invalid_argument("invalid NBC cardinality enumeration request");
        if (all_future_covered_) return enumeration_result::exhausted;

        solver_ptr solver = make_solver(false);
        if (!solver) return enumeration_result::exhausted;

        std::vector<int> assumptions{cardinality_outputs_[cardinality - 1]};
        if (cardinality < dimension_) assumptions.push_back(-cardinality_outputs_[cardinality]);
        visitor_ = visit;
        visitor_state_ = state;
        callback_exception_ = nullptr;
        const int result = nbc_solver_enumerate(solver.get(), assumptions.data(), static_cast<int>(assumptions.size()),
                                                &model_callback, this, &terminate_callback, nullptr);
        visitor_ = nullptr;
        visitor_state_ = nullptr;
        if (callback_exception_ != nullptr) std::rethrow_exception(callback_exception_);
        if (result == NBC_ENUM_INTERRUPTED) {
            timeout_checkpoint();
            throw std::runtime_error("NBC enumeration was interrupted without a coposit timeout");
        }
        if (result == NBC_ENUM_ERROR) throw std::runtime_error("NBC enumeration failed");
        return result == NBC_ENUM_STOPPED ? enumeration_result::stopped : enumeration_result::exhausted;
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
        void operator()(nbc_solver* solver) const noexcept { nbc_solver_delete(solver); }
    };

    using solver_ptr = std::unique_ptr<nbc_solver, solver_deleter>;

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

    void release_interval(interval& value) noexcept
    {
        support_context_.release(std::move(value.lower));
        support_context_.release(std::move(value.upper));
    }

    void clear_intervals(std::vector<interval>& values) noexcept
    {
        for (interval& value : values) release_interval(value);
        values.clear();
    }

    void clear_supports(std::vector<support>& values) noexcept
    {
        for (support& value : values) support_context_.release(std::move(value));
        values.clear();
    }

    void clear_prefixes(std::vector<prefix>& values) noexcept
    {
        for (prefix& value : values) support_context_.release(std::move(value.values));
        values.clear();
    }

    static bool capture_first_support(void* opaque, const std::vector<size_t>& indices)
    {
        *static_cast<std::vector<size_t>*>(opaque) = indices;
        return false;
    }

    solver_ptr make_solver(bool include_pending)
    {
        solver_ptr solver(nbc_solver_new());
        if (!solver) throw std::bad_alloc();
        nbc_solver_set_variable_count(solver.get(), next_variable_ - 1);
        for (const auto& clause : base_clauses_) {
            const int result = nbc_solver_add_clause(solver.get(), clause.data(), static_cast<int>(clause.size()));
            if (result < 0) throw std::runtime_error("NBC rejected an internal cardinality clause");
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

    bool install_interval(nbc_solver* solver, const interval& certificate)
    {
        if (solver == nullptr) return true;
        clause_.clear();
        for (size_t index = 0; index < dimension_; ++index) {
            if (support_context_.contains(certificate.lower, index)) clause_.push_back(-variable(index));
            else if (!support_context_.contains(certificate.upper, index)) clause_.push_back(variable(index));
        }
        if (certificate.upper_size < dimension_) clause_.push_back(cardinality_outputs_[certificate.upper_size]);
        if (clause_.empty()) return false;
        const int result = nbc_solver_add_clause(solver, clause_.data(), static_cast<int>(clause_.size()));
        if (result < 0) throw std::runtime_error("NBC rejected a retained support certificate");
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
                throw std::runtime_error("NBC returned an incomplete cardinality-network assignment");
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
            throw std::overflow_error("NBC cardinality network exceeds its integer literal range");
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
        support ceiling = support_context_.make();
        support_context_.set_all(ceiling);
        for (interval& certificate : active_) {
            if (support_context_.equal(certificate.upper, ceiling)) {
                roots.push_back(std::move(certificate.lower));
                support_context_.release(std::move(certificate.upper));
            }
            else bounded.push_back(std::move(certificate));
        }
        active_.clear();

        if (std::any_of(roots.begin(), roots.end(), [&](const support& root) { return support_context_.empty(root); })) {
            all_future_covered_ = true;
            clear_supports(roots);
            clear_intervals(bounded);
            support_context_.release(std::move(ceiling));
            return;
        }

        bool changed = true;
        std::vector<size_t> bits;
        while (changed && !roots.empty()) {
            changed = false;
            std::sort(roots.begin(), roots.end(), [&](const support& left, const support& right) {
                const size_t left_size = support_context_.count(left);
                const size_t right_size = support_context_.count(right);
                return left_size != right_size ? left_size < right_size : support_context_.less(left, right);
            });
            std::vector<support> minimal;
            std::vector<std::vector<size_t>> minimal_by_bit(dimension_);
            for (support& candidate : roots) {
                bool covered = false;
                support_context_.extract_set_indices(candidate, bits);
                for (const size_t bit : bits) {
                    for (const size_t retained : minimal_by_bit[bit]) {
                        if (support_context_.is_subset_of(minimal[retained], candidate)) {
                            covered = true;
                            break;
                        }
                    }
                    if (covered) break;
                }
                if (covered) {
                    support_context_.release(std::move(candidate));
                    continue;
                }
                const size_t retained = minimal.size();
                minimal.push_back(std::move(candidate));
                minimal_by_bit[support_context_.first(minimal[retained])].push_back(retained);
            }
            roots.swap(minimal);

            std::map<support, size_t, support_less> parent_counts(support_less{&support_context_});
            for (const support& root : roots) {
                const size_t root_size = support_context_.count(root);
                if (root_size == 0 || root_size > completed_cardinality) continue;
                support parent = support_context_.clone(root);
                support_context_.extract_set_indices(root, bits);
                for (const size_t bit : bits) {
                    support_context_.reset(parent, bit);
                    const auto found = parent_counts.find(parent);
                    if (found == parent_counts.end()) parent_counts.emplace(support_context_.clone(parent), 1);
                    else ++found->second;
                    support_context_.set(parent, bit);
                }
                support_context_.release(std::move(parent));
            }
            while (!parent_counts.empty()) {
                auto node = parent_counts.extract(parent_counts.begin());
                support& parent = node.key();
                if (node.mapped() != dimension_ - support_context_.count(parent)) {
                    support_context_.release(std::move(parent));
                    continue;
                }
                if (support_context_.empty(parent)) {
                    all_future_covered_ = true;
                    support_context_.release(std::move(parent));
                    clear_supports(roots);
                    clear_intervals(bounded);
                    support_context_.release(std::move(ceiling));
                    return;
                }
                roots.push_back(std::move(parent));
                changed = true;
            }
        }

        active_ = std::move(bounded);
        for (support& root : roots)
            active_.push_back({std::move(root), support_context_.clone(ceiling), 0, dimension_});
        for (interval& certificate : active_) certificate.lower_size = support_context_.count(certificate.lower);
        support_context_.release(std::move(ceiling));
    }

    void compact_contained_intervals(size_t first_remaining_cardinality, size_t last_remaining_cardinality)
    {
        std::vector<interval> retained;
        retained.reserve(active_.size());
        for (interval& certificate : active_) {
            if (certificate.upper_size < first_remaining_cardinality || certificate.lower_size > last_remaining_cardinality)
                release_interval(certificate);
            else retained.push_back(std::move(certificate));
        }
        active_.swap(retained);
        if (active_.size() < 2) return;
        std::sort(active_.begin(), active_.end(), [&](const interval& left, const interval& right) {
            if (left.lower_size != right.lower_size) return left.lower_size < right.lower_size;
            if (left.upper_size != right.upper_size) return left.upper_size > right.upper_size;
            if (!support_context_.equal(left.lower, right.lower)) return support_context_.less(left.lower, right.lower);
            return support_context_.less(left.upper, right.upper);
        });

        std::vector<interval> compacted;
        std::vector<std::vector<size_t>> by_lower_bit(dimension_ + 1);
        std::vector<size_t> bits;
        compacted.reserve(active_.size());
        for (interval& candidate : active_) {
            bool covered = false;
            for (const size_t retained_index : by_lower_bit[dimension_]) {
                const interval& retained = compacted[retained_index];
                if (retained.upper_size >= candidate.upper_size && contains(support_context_, retained, candidate)) {
                    covered = true;
                    break;
                }
            }
            support_context_.extract_set_indices(candidate.lower, bits);
            for (const size_t bit : bits) {
                if (covered) break;
                for (const size_t retained_index : by_lower_bit[bit]) {
                    const interval& retained = compacted[retained_index];
                    if (retained.lower_size <= candidate.lower_size && retained.upper_size >= candidate.upper_size
                        && contains(support_context_, retained, candidate)) {
                        covered = true;
                        break;
                    }
                }
                if (covered) break;
            }
            if (covered) {
                release_interval(candidate);
                continue;
            }
            const size_t index = compacted.size();
            compacted.push_back(std::move(candidate));
            const size_t bucket = support_context_.empty(compacted[index].lower) ? dimension_ : support_context_.first(compacted[index].lower);
            by_lower_bit[bucket].push_back(index);
        }
        active_.swap(compacted);
    }


    support_context& support_context_;
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

nbc_upward_supports::nbc_upward_supports(support_context& context)
    : impl_(std::make_unique<impl>(context))
{
}

nbc_upward_supports::~nbc_upward_supports() = default;

void nbc_upward_supports::add_interval(const support& lower, const support& upper) { impl_->add_interval(lower, upper); }
void nbc_upward_supports::add_pair_upward_closure(size_t first, size_t second) { impl_->add_pair_upward_closure(first, second); }
void nbc_upward_supports::add_upward_closure(const std::vector<size_t>& indices) { impl_->add_upward_closure(indices); }

void nbc_upward_supports::start_cardinality(size_t cardinality, bool high_frontier)
{
    impl_->start_cardinality(cardinality, high_frontier);
}

bool nbc_upward_supports::take_first(std::vector<size_t>& indices, bool high_frontier)
{
    return impl_->take_first(indices, high_frontier);
}

nbc_upward_supports::enumeration_result nbc_upward_supports::enumerate_cardinality(
    size_t cardinality, void* state, visitor visit)
{
    return impl_->enumerate_cardinality(cardinality, state, visit);
}

void nbc_upward_supports::commit_layer(size_t completed_cardinality) { impl_->commit_layer(completed_cardinality); }
void nbc_upward_supports::commit_frontiers(size_t first_remaining_cardinality, size_t last_remaining_cardinality)
{
    impl_->commit_frontiers(first_remaining_cardinality, last_remaining_cardinality);
}
size_t nbc_upward_supports::interval_count() const noexcept { return impl_->interval_count(); }
bool nbc_upward_supports::all_future_covered() const noexcept { return impl_->all_future_covered(); }

} // namespace coposit
