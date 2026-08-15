#include <coposit/fraction_free_ldlt.hpp>
#include <coposit/model.hpp>
#include <coposit/progress.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include "source_trace.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pthread.h>
#include <sched.h>

namespace coposit::model {

namespace {

struct subset_result {
    explicit subset_result(size_t dimension)
        : lower(dimension)
        , upper(dimension)
    {
    }

    void reset() noexcept
    {
        ready = false;
        negative_witness = false;
        nonnegative_zero = false;
        lower.clear();
        upper.clear();
    }

    bool ready = false;
    bool negative_witness = false;
    bool nonnegative_zero = false;
    support lower;
    support upper;
};

class interval_cbdd {
public:
    explicit interval_cbdd(size_t dimension, progress::tracker* progress = nullptr)
        : dimension_(dimension)
        , progress_(progress)
        , current_support_(dimension)
    {
        nodes_.push_back({dimension_, dimension_, 0, 0}); // Constant false.
        nodes_.push_back({dimension_, dimension_, 1, 1}); // Constant true.
    }

    void start_cardinality(size_t cardinality)
    {
        if (!progress_) {
            remaining_ = subtract(cardinality_family(cardinality), covered_);
            return;
        }
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::cardinality_build);
        remaining_ = subtract(cardinality_family(cardinality), covered_);
        publish_work();
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::support_solve);
    }

    void take_batch(size_t limit, std::vector<support>& result)
    {
        result.clear();
        if (remaining_ == empty || limit == 0) return;
        current_support_.clear();
        enumerate(remaining_, 0, limit, result);
    }

    void add_intervals(const std::vector<subset_result>& results, size_t count)
    {
        union_cache_.clear();
        size_t certificate = empty;
        for (size_t index = 0; index < count; ++index) {
            if (!results[index].ready || results[index].negative_witness) continue;
            certificate = progress_ ? unite_impl<true>(certificate, interval_family(results[index].lower, results[index].upper))
                                    : unite_impl<false>(certificate, interval_family(results[index].lower, results[index].upper));
        }
        if (certificate == empty) return;

        if (!progress_) {
            covered_ = unite(covered_, certificate);
            remaining_ = subtract(remaining_, certificate);
            return;
        }
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::certificate_union);
        covered_ = unite(covered_, certificate);
        publish_work();
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::certificate_subtract);
        remaining_ = subtract(remaining_, certificate);
        publish_work();
        progress_->decision_diagram_phase_change(progress::decision_diagram_phase::support_solve);
    }

    size_t node_count() const noexcept { return nodes_.size(); }

    size_t maximum_chain_length() const noexcept
    {
        size_t result = 0;
        for (size_t root = 2; root < nodes_.size(); ++root)
            result = std::max(result, nodes_[root].bottom - nodes_[root].top + 1);
        return result;
    }

private:
    static constexpr size_t empty = 0;
    static constexpr size_t unit = 1;

    struct node {
        size_t top;
        size_t bottom;
        size_t low;
        size_t high;
    };

    struct node_key {
        size_t top;
        size_t bottom;
        size_t low;
        size_t high;

        bool operator==(const node_key& other) const noexcept
        {
            return top == other.top && bottom == other.bottom && low == other.low && high == other.high;
        }
    };

    struct node_key_hash {
        size_t operator()(const node_key& value) const noexcept
        {
            size_t result = std::hash<size_t>{}(value.top);
            result ^= std::hash<size_t>{}(value.bottom) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            result ^= std::hash<size_t>{}(value.low) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            result ^= std::hash<size_t>{}(value.high) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);
            return result;
        }
    };

    struct pair_key {
        size_t left;
        size_t right;

        bool operator==(const pair_key& other) const noexcept
        {
            return left == other.left && right == other.right;
        }
    };

    struct pair_key_hash {
        size_t operator()(const pair_key& value) const noexcept
        {
            size_t result = std::hash<size_t>{}(value.left);
            return result ^ (std::hash<size_t>{}(value.right) + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2));
        }
    };

    size_t actual_index(size_t variable) const noexcept
    {
        return dimension_ - 1 - variable;
    }

    size_t top(size_t root) const noexcept
    {
        return root < 2 ? dimension_ : nodes_[root].top;
    }

    void enumerate(size_t root, size_t variable, size_t limit, std::vector<support>& result)
    {
        if (root == empty || result.size() == limit) return;
        if ((++enumeration_steps_ & 4095U) == 0) timeout_checkpoint();
        if (variable == dimension_) {
            assert(root == unit);
            result.push_back(current_support_);
            return;
        }

        if (root == unit || variable < nodes_[root].top) {
            enumerate(root, variable + 1, limit, result);
            if (result.size() == limit) return;
            current_support_.set(actual_index(variable));
            enumerate(root, variable + 1, limit, result);
            current_support_.reset(actual_index(variable));
            return;
        }

        const node value = nodes_[root];
        assert(variable <= value.bottom);
        enumerate(variable == value.bottom ? value.low : root, variable + 1, limit, result);
        if (result.size() == limit) return;
        current_support_.set(actual_index(variable));
        enumerate(value.high, variable + 1, limit, result);
        current_support_.reset(actual_index(variable));
    }

    template <bool ReportProgress>
    void operation_checkpoint()
    {
        if constexpr (ReportProgress) {
            ++operations_;
            if (operations_ % progress::decision_diagram_publish_interval == 0) publish_work();
            if ((operations_ & 4095U) == 0) timeout_checkpoint();
        } else {
            if ((++operations_ & 4095U) == 0) timeout_checkpoint();
        }
    }

    void publish_work() noexcept
    {
        if (progress_) progress_->decision_diagram_work(nodes_.size(), operations_);
    }

    size_t make_node(size_t top_value, size_t bottom_value, size_t low, size_t high)
    {
        if (low == high) return low;

        if (low >= 2) {
            const node child = nodes_[low];
            if (child.top == bottom_value + 1 && child.high == high)
                return make_node(top_value, child.bottom, child.low, high);
        }

        const node_key key{top_value, bottom_value, low, high};
        const auto found = unique_.find(key);
        if (found != unique_.end()) return found->second;

        const size_t result = nodes_.size();
        nodes_.push_back({top_value, bottom_value, low, high});
        unique_.emplace(key, result);
        return result;
    }

    size_t make_node(size_t variable_value, size_t low, size_t high)
    {
        return make_node(variable_value, variable_value, low, high);
    }

    size_t interval_family(const support& lower, const support& upper)
    {
        size_t root = unit;
        for (size_t variable_value = dimension_; variable_value-- > 0;) {
            const size_t bit = actual_index(variable_value);
            if (lower.contains(bit)) {
                root = make_node(variable_value, empty, root);
            } else if (!upper.contains(bit)) {
                root = make_node(variable_value, root, empty);
            }
        }
        return root;
    }

    size_t cardinality_family(size_t cardinality)
    {
        const size_t missing = std::numeric_limits<size_t>::max();
        std::vector<std::vector<size_t>> memo(dimension_ + 1, std::vector<size_t>(cardinality + 1, missing));
        std::function<size_t(size_t, size_t)> build = [&](size_t variable_value, size_t needed) -> size_t {
            if (needed > dimension_ - variable_value) return empty;
            if (variable_value == dimension_) return needed == 0 ? unit : empty;
            size_t& cached = memo[variable_value][needed];
            if (cached != missing) return cached;

            const size_t low = build(variable_value + 1, needed);
            const size_t high = needed == 0 ? empty : build(variable_value + 1, needed - 1);
            cached = make_node(variable_value, low, high);
            return cached;
        };
        return build(0, cardinality);
    }

    size_t unite(size_t left, size_t right)
    {
        union_cache_.clear();
        return progress_ ? unite_impl<true>(left, right) : unite_impl<false>(left, right);
    }

    template <bool ReportProgress>
    size_t unite_impl(size_t left, size_t right)
    {
        operation_checkpoint<ReportProgress>();
        if (left == unit || right == unit) return unit;
        if (left == empty || left == right) return right;
        if (right == empty) return left;
        if (right < left) std::swap(left, right);

        const pair_key key{left, right};
        const auto found = union_cache_.find(key);
        if (found != union_cache_.end()) return found->second;

        const size_t top_value = std::min(top(left), top(right));
        const size_t bottom_value = std::min(split_bottom(left, top_value), split_bottom(right, top_value));
        const auto [left_low, left_high] = cofactors(left, bottom_value);
        const auto [right_low, right_high] = cofactors(right, bottom_value);
        const size_t result = make_node(top_value,
                                        bottom_value,
                                        unite_impl<ReportProgress>(left_low, right_low),
                                        unite_impl<ReportProgress>(left_high, right_high));
        union_cache_.emplace(key, result);
        return result;
    }

    size_t subtract(size_t left, size_t right)
    {
        difference_cache_.clear();
        return progress_ ? subtract_impl<true>(left, right) : subtract_impl<false>(left, right);
    }

    template <bool ReportProgress>
    size_t subtract_impl(size_t left, size_t right)
    {
        operation_checkpoint<ReportProgress>();
        if (left == empty || left == right) return empty;
        if (right == empty) return left;
        if (right == unit) return empty;

        const pair_key key{left, right};
        const auto found = difference_cache_.find(key);
        if (found != difference_cache_.end()) return found->second;

        const size_t top_value = std::min(top(left), top(right));
        const size_t bottom_value = std::min(split_bottom(left, top_value), split_bottom(right, top_value));
        const auto [left_low, left_high] = cofactors(left, bottom_value);
        const auto [right_low, right_high] = cofactors(right, bottom_value);
        const size_t result = make_node(top_value,
                                        bottom_value,
                                        subtract_impl<ReportProgress>(left_low, right_low),
                                        subtract_impl<ReportProgress>(left_high, right_high));
        difference_cache_.emplace(key, result);
        return result;
    }

    size_t split_bottom(size_t root, size_t top_value) const noexcept
    {
        if (top(root) == top_value) return nodes_[root].bottom;
        if (root < 2) return dimension_;
        return nodes_[root].top - 1;
    }

    std::pair<size_t, size_t> cofactors(size_t root, size_t bottom_value)
    {
        if (bottom_value < top(root)) return {root, root};

        const node value = nodes_[root];
        if (bottom_value == value.bottom) return {value.low, value.high};
        return {make_node(bottom_value + 1, value.bottom, value.low, value.high), value.high};
    }

    size_t dimension_;
    progress::tracker* progress_;
    std::vector<node> nodes_;
    std::unordered_map<node_key, size_t, node_key_hash> unique_;
    std::unordered_map<pair_key, size_t, pair_key_hash> union_cache_;
    std::unordered_map<pair_key, size_t, pair_key_hash> difference_cache_;
    size_t covered_ = empty;
    size_t remaining_ = empty;
    support current_support_;
    uint64_t operations_ = 0;
    uint64_t enumeration_steps_ = 0;
};

struct z_search_task {
    support block;
    support candidates;
    support excluded;
    size_t block_size = 0;
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

    std::vector<z_search_task> partition(size_t target) const
    {
        const size_t dimension = adjacency_.size();
        support block(dimension);
        support candidates(dimension);
        support excluded(dimension);
        candidates.set_all();

        std::deque<z_search_task> frontier;
        std::vector<z_search_task> tasks;
        frontier.push_back({std::move(block), std::move(candidates), std::move(excluded), 0});
        while (!frontier.empty() && tasks.size() + frontier.size() < target) {
            z_search_task task = std::move(frontier.front());
            frontier.pop_front();
            if (!expand(task, frontier)) tasks.push_back(std::move(task));
        }
        while (!frontier.empty()) {
            tasks.push_back(std::move(frontier.front()));
            frontier.pop_front();
        }
        return tasks;
    }

    template <class Visitor>
    bool visit(const z_search_task& task, const std::atomic<bool>& cancelled, Visitor&& visitor) const
    {
        support block = task.block;
        return search(block, task.candidates, task.excluded, task.block_size, cancelled, visitor);
    }

private:
    bool expand(const z_search_task& task, std::deque<z_search_task>& children) const
    {
        if (task.candidates.empty() && task.excluded.empty()) return false;

        support candidates = task.candidates;
        support excluded = task.excluded;
        const size_t pivot = !candidates.empty() ? candidates.lowest_index() : excluded.lowest_index();
        support extensions = candidates;
        extensions.remove(adjacency_[pivot]);
        std::vector<size_t> vertices;
        extensions.copy_indices_to(vertices);

        for (const size_t vertex : vertices) {
            support child_block = task.block;
            support child_candidates = candidates;
            support child_excluded = excluded;
            child_block.set(vertex);
            child_candidates.intersect_with(adjacency_[vertex]);
            child_excluded.intersect_with(adjacency_[vertex]);
            children.push_back(
                {std::move(child_block), std::move(child_candidates), std::move(child_excluded), task.block_size + 1});
            candidates.reset(vertex);
            excluded.set(vertex);
        }
        return !vertices.empty();
    }

    template <class Visitor>
    bool search(support& block, support candidates, support excluded, size_t block_size, const std::atomic<bool>& cancelled,
                Visitor& visitor) const
    {
        timeout_checkpoint();
        if (cancelled.load(std::memory_order_relaxed)) return false;
        if (candidates.empty() && excluded.empty()) return block_size < 2 || visitor(block, block_size);

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
            if (!search(block, std::move(child_candidates), std::move(child_excluded), block_size + 1, cancelled, visitor))
                return false;
            block.reset(vertex);
            candidates.reset(vertex);
            excluded.set(vertex);
        }
        return true;
    }

    std::vector<support> adjacency_;
};

void copy_principal(const matrix_integer& matrix, const std::vector<size_t>& indices, matrix_integer& principal)
{
    for (size_t row = 0; row < indices.size(); ++row) {
        timeout_checkpoint();
        for (size_t column = 0; column <= row; ++column) principal(row, column) = matrix(indices[row], indices[column]);
    }
}

class z_block_evaluator {
public:
    explicit z_block_evaluator(size_t dimension)
        : factorization_(dimension)
    {
        indices_.reserve(dimension);
        queue_.reserve(dimension);
        component_.reserve(dimension);
    }

    copositivity_classification evaluate(const matrix_integer& matrix, const support& block)
    {
        block.copy_indices_to(indices_);
        reached_.assign(indices_.size(), false);
        copositivity_classification result{true, true};
        for (size_t start = 0; start < indices_.size(); ++start) {
            if (reached_[start]) continue;
            queue_.assign(1, start);
            reached_[start] = true;
            component_.clear();
            for (size_t next = 0; next < queue_.size(); ++next) {
                const size_t local = queue_[next];
                component_.push_back(indices_[local]);
                for (size_t candidate = 0; candidate < indices_.size(); ++candidate) {
                    if (!reached_[candidate] && matrix(indices_[local], indices_[candidate]).sign() < 0) {
                        reached_[candidate] = true;
                        queue_.push_back(candidate);
                    }
                }
            }

            principal_.resize(component_.size(), component_.size());
            copy_principal(matrix, component_, principal_);
            factorization_.factorize_inplace(principal_);
            COPOSIT_MULTITHREADED_CBDD_ZED_TRACE("z-component", component_.size());
            const bool positive_semidefinite = factorization_.is_positive_semidefinite();
            if (!positive_semidefinite) return {false, false};
            result.is_strictly_copositive &= factorization_.is_positive_definite();
        }
        return result;
    }

private:
    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    std::vector<size_t> indices_;
    std::vector<bool> reached_;
    std::vector<size_t> queue_;
    std::vector<size_t> component_;
};

class subset_evaluator {
public:
    explicit subset_evaluator(size_t dimension)
        : factorization_(dimension)
        , product_(dimension)
    {
        indices_.reserve(dimension);
    }

    void evaluate(const matrix_integer& matrix, const support& selected, subset_result& result)
    {
        result.reset();
        selected.copy_indices_to(indices_);
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
        result.nonnegative_zero = singular;
        for (size_t row = 0; row < dimension; ++row) {
            all_nonpositive &= solution_(row, 0).sign() <= 0;
            result.nonnegative_zero &= solution_(row, 0).sign() >= 0;
        }
        if (all_nonpositive) {
            result.negative_witness = true;
            result.ready = true;
            return;
        }

        for (size_t local = 0; local < indices_.size(); ++local)
            if (!solution_(local, 0).is_zero()) result.lower.set(indices_[local]);

        for (integer& value : product_) value.set_zero();
        for (size_t row = 0; row < matrix.rows(); ++row) {
            timeout_checkpoint();
            for (size_t local = 0; local < indices_.size(); ++local)
                product_[row].addmul(matrix(row, indices_[local]), solution_(local, 0));
            if (product_[row].sign() >= 0) result.upper.set(row);
        }
        result.ready = true;
    }

private:
    fraction_free_ldlt_factorization factorization_;
    matrix_integer principal_;
    matrix_integer solution_;
    std::vector<integer> product_;
    std::vector<size_t> indices_;
};

class support_worker_pool {
public:
    support_worker_pool(size_t dimension, size_t worker_count, size_t first_cpu)
        : dimension_(dimension)
    {
        evaluators_.reserve(worker_count);
        for (size_t index = 0; index < worker_count; ++index) evaluators_.push_back(std::make_unique<worker_evaluators>(dimension));
        threads_.reserve(worker_count);
        try {
            for (size_t index = 0; index < worker_count; ++index) {
                if (first_cpu > std::numeric_limits<size_t>::max() - index)
                    throw std::invalid_argument("CBDD worker CPU index is too large");
                threads_.emplace_back([this, index] { worker_loop(*evaluators_[index]); });
                pin(threads_.back(), first_cpu + index);
            }
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stop_ = true;
                ++generation_;
            }
            work_available_.notify_all();
            for (std::thread& thread : threads_) thread.join();
            throw;
        }
    }

    support_worker_pool(const support_worker_pool&) = delete;
    support_worker_pool& operator=(const support_worker_pool&) = delete;

    ~support_worker_pool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
            ++generation_;
        }
        work_available_.notify_all();
        for (std::thread& thread : threads_) thread.join();
    }

    const std::vector<subset_result>& evaluate(
        const matrix_integer& matrix, const std::vector<support>& batch, bool stop_on_nonnegative_zero)
    {
        while (results_.size() < batch.size()) results_.emplace_back(dimension_);
        for (size_t index = 0; index < batch.size(); ++index) results_[index].reset();

        std::unique_lock<std::mutex> lock(mutex_);
        matrix_ = &matrix;
        batch_ = &batch;
        batch_size_ = batch.size();
        work_ = work_kind::supports;
        stop_on_nonnegative_zero_ = stop_on_nonnegative_zero;
        next_.store(0, std::memory_order_relaxed);
        cancel_.store(false, std::memory_order_relaxed);
        failure_ = nullptr;
        workers_left_ = threads_.size();
        ++generation_;
        work_available_.notify_all();
        work_finished_.wait(lock, [this] { return workers_left_ == 0; });
        bool decisive = false;
        for (size_t index = 0; index < batch.size(); ++index)
            decisive |= results_[index].ready
                        && (results_[index].negative_witness
                            || (stop_on_nonnegative_zero && results_[index].nonnegative_zero));
        if (failure_ && !decisive) std::rethrow_exception(failure_);
        return results_;
    }

    copositivity_classification evaluate_zed(const matrix_integer& matrix, const maximal_z_blocks& blocks,
                                             const std::vector<z_search_task>& tasks, copositivity_mode mode,
                                             bool combined, progress::tracker& progress)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        matrix_ = &matrix;
        zed_blocks_ = &blocks;
        zed_tasks_ = &tasks;
        batch_size_ = tasks.size();
        zed_mode_ = mode;
        zed_combined_ = combined;
        zed_completed_.store(0, std::memory_order_relaxed);
        zed_rejected_.store(false, std::memory_order_relaxed);
        zed_strict_.store(true, std::memory_order_relaxed);
        next_.store(0, std::memory_order_relaxed);
        cancel_.store(false, std::memory_order_relaxed);
        failure_ = nullptr;
        workers_left_ = threads_.size();
        work_ = work_kind::zed;
        ++generation_;
        work_available_.notify_all();

        uint64_t published = 0;
        if (progress.active()) {
            while (!work_finished_.wait_for(lock, std::chrono::milliseconds(100), [this] { return workers_left_ == 0; })) {
                const uint64_t completed = zed_completed_.load(std::memory_order_relaxed);
                progress.decision_diagram_zed_blocks(completed - published);
                published = completed;
            }
        } else {
            work_finished_.wait(lock, [this] { return workers_left_ == 0; });
        }
        const uint64_t completed = zed_completed_.load(std::memory_order_relaxed);
        progress.decision_diagram_zed_blocks(completed - published);

        const bool rejected = zed_rejected_.load(std::memory_order_relaxed);
        if (failure_ && !rejected) std::rethrow_exception(failure_);
        if (rejected) return {false, false};
        return {true, zed_strict_.load(std::memory_order_relaxed)};
    }

private:
    struct worker_evaluators {
        explicit worker_evaluators(size_t dimension)
            : subset(dimension)
            , zed(dimension)
        {
        }

        subset_evaluator subset;
        z_block_evaluator zed;
    };

    enum class work_kind { supports, zed };

    static void pin(std::thread& thread, size_t cpu)
    {
        if (cpu >= CPU_SETSIZE) throw std::invalid_argument("CBDD worker CPU index exceeds CPU_SETSIZE");
        cpu_set_t selected;
        CPU_ZERO(&selected);
        CPU_SET(cpu, &selected);
        const int error = pthread_setaffinity_np(thread.native_handle(), sizeof(selected), &selected);
        if (error != 0) throw std::system_error(error, std::generic_category(), "Could not pin CBDD worker thread");
    }

    void worker_loop(worker_evaluators& evaluators)
    {
        size_t observed_generation = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                work_available_.wait(lock, [this, &observed_generation] { return stop_ || generation_ != observed_generation; });
                if (stop_) return;
                observed_generation = generation_;
            }

            while (!cancel_.load(std::memory_order_relaxed)) {
                const size_t index = next_.fetch_add(1, std::memory_order_relaxed);
                if (index >= batch_size_) break;
                try {
                    if (work_ == work_kind::supports) {
                        evaluators.subset.evaluate(*matrix_, (*batch_)[index], results_[index]);
                        if (results_[index].negative_witness
                            || (stop_on_nonnegative_zero_ && results_[index].nonnegative_zero))
                            cancel_.store(true, std::memory_order_relaxed);
                    } else {
                        zed_blocks_->visit((*zed_tasks_)[index], cancel_, [&](const support& block, size_t block_size) {
                            const copositivity_classification result = evaluators.zed.evaluate(*matrix_, block);
                            zed_completed_.fetch_add(1, std::memory_order_relaxed);
                            if (!result.is_strictly_copositive) zed_strict_.store(false, std::memory_order_relaxed);
                            const bool accepted = zed_combined_
                                                      ? result.is_copositive
                                                      : (zed_mode_ == copositivity_mode::strictly_copositive
                                                             ? result.is_strictly_copositive
                                                             : result.is_copositive);
                            COPOSIT_MULTITHREADED_CBDD_ZED_TRACE(accepted ? "z-pass" : "z-reject", block_size);
                            if (!accepted) {
                                zed_rejected_.store(true, std::memory_order_relaxed);
                                cancel_.store(true, std::memory_order_relaxed);
                            }
                            return accepted;
                        });
                    }
                } catch (...) {
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (!failure_) failure_ = std::current_exception();
                    }
                    cancel_.store(true, std::memory_order_relaxed);
                }
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (--workers_left_ == 0) work_finished_.notify_one();
            }
        }
    }

    const size_t dimension_;
    std::vector<std::unique_ptr<worker_evaluators>> evaluators_;
    std::vector<std::thread> threads_;
    std::vector<subset_result> results_;
    std::mutex mutex_;
    std::condition_variable work_available_;
    std::condition_variable work_finished_;
    const matrix_integer* matrix_ = nullptr;
    const std::vector<support>* batch_ = nullptr;
    const maximal_z_blocks* zed_blocks_ = nullptr;
    const std::vector<z_search_task>* zed_tasks_ = nullptr;
    size_t batch_size_ = 0;
    size_t workers_left_ = 0;
    size_t generation_ = 0;
    std::atomic<size_t> next_{0};
    std::atomic<bool> cancel_{false};
    std::atomic<uint64_t> zed_completed_{0};
    std::atomic<bool> zed_rejected_{false};
    std::atomic<bool> zed_strict_{true};
    work_kind work_ = work_kind::supports;
    copositivity_mode zed_mode_ = copositivity_mode::strictly_copositive;
    bool zed_combined_ = false;
    bool stop_on_nonnegative_zero_ = false;
    bool stop_ = false;
    std::exception_ptr failure_;
};

size_t configured_worker_count()
{
    constexpr size_t default_workers = 7;
    const char* text = std::getenv("COPOSIT_CBDD_WORKERS");
    if (text == nullptr) return default_workers;

    const std::string_view value(text);
    size_t workers = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), workers);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || workers == 0)
        throw std::invalid_argument("COPOSIT_CBDD_WORKERS must be a positive integer");
    return workers;
}

size_t configured_first_cpu()
{
    constexpr size_t default_first_cpu = 3;
    const char* text = std::getenv("COPOSIT_CBDD_FIRST_CPU");
    if (text == nullptr) return default_first_cpu;

    const std::string_view value(text);
    size_t first_cpu = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), first_cpu);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::invalid_argument("COPOSIT_CBDD_FIRST_CPU must be a nonnegative integer");
    return first_cpu;
}

bool configured_zed_scan()
{
    const char* text = std::getenv("COPOSIT_CBDD_ZED_SCAN");
    if (text == nullptr || std::string_view(text) == "on") return true;
    if (std::string_view(text) == "off") return false;
    throw std::invalid_argument("COPOSIT_CBDD_ZED_SCAN must be 'on' or 'off'");
}

size_t batch_size(size_t dimension, size_t workers) noexcept
{
    const size_t raw = dimension > std::numeric_limits<size_t>::max() / 5 ? std::numeric_limits<size_t>::max() : 5 * dimension;
    return raw < workers ? raw : raw - raw % workers;
}

class dickinson_checker {
public:
    dickinson_checker(size_t dimension, copositivity_mode mode)
        : mode_(mode)
        , progress_(progress::metric::decision_diagram, dimension)
        , supports_(dimension, progress_.active() ? &progress_ : nullptr)
    {
    }

    dickinson_checker(size_t dimension, copositivity_classification& classification)
        : mode_(copositivity_mode::copositive)
        , classification_(&classification)
        , progress_(progress::metric::decision_diagram, dimension)
        , supports_(dimension, progress_.active() ? &progress_ : nullptr)
    {
    }

    bool check(const matrix_integer& matrix)
    {
        const size_t workers = std::min(configured_worker_count(), batch_size(matrix.rows(), 1));
        support_worker_pool pool(matrix.rows(), workers, configured_first_cpu());

        const size_t batch_limit = batch_size(matrix.rows(), workers);
        std::vector<support> batch;
        batch.reserve(batch_limit);

        for (size_t subset_dimension = 1; subset_dimension <= matrix.rows(); ++subset_dimension) {
            progress_.decision_diagram_cardinality(subset_dimension, progress::decision_diagram_phase::cardinality_build);
            supports_.start_cardinality(subset_dimension);
            for (;;) {
                timeout_checkpoint();
                supports_.take_batch(batch_limit, batch);
                if (batch.empty()) break;

                const bool reject_nonnegative_zero = classification_ == nullptr && mode_ == copositivity_mode::strictly_copositive;
                const std::vector<subset_result>& results = pool.evaluate(matrix, batch, reject_nonnegative_zero);
                for (size_t index = 0; index < batch.size(); ++index) {
                    if (!results[index].ready) continue;
                    progress_.decision_diagram_support();
                    COPOSIT_MULTITHREADED_CBDD_ZED_TRACE("process", subset_dimension);
                    if (results[index].negative_witness || (reject_nonnegative_zero && results[index].nonnegative_zero)) {
                        progress_.finish();
                        return false;
                    }
                    if (classification_ != nullptr && results[index].nonnegative_zero)
                        classification_->is_strictly_copositive = false;
                    progress_.decision_diagram_certificate();
                }
                supports_.add_intervals(results, batch.size());
            }
        }

        progress_.finish();
        return true;
    }

private:
    const copositivity_mode mode_;
    copositivity_classification* classification_ = nullptr;
    progress::tracker progress_;
    interval_cbdd supports_;
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

#ifdef COPOSIT_MULTITHREADED_CBDD_ZED_DICKINSON_TESTING
std::pair<size_t, size_t> multithreaded_cbdd_zed_uncovered_count(
    size_t dimension, size_t cardinality, const std::vector<std::pair<uint64_t, uint64_t>>& intervals)
{
    interval_cbdd diagram(dimension);
    std::vector<subset_result> certificates;
    certificates.reserve(intervals.size());
    for (const auto& [lower_mask, upper_mask] : intervals) {
        certificates.emplace_back(dimension);
        subset_result& certificate = certificates.back();
        certificate.ready = true;
        for (size_t bit = 0; bit < dimension; ++bit) {
            if ((lower_mask & (uint64_t{1} << bit)) != 0) certificate.lower.set(bit);
            if ((upper_mask & (uint64_t{1} << bit)) != 0) certificate.upper.set(bit);
        }
    }
    diagram.add_intervals(certificates, certificates.size());

    diagram.start_cardinality(cardinality);
    std::vector<support> uncovered;
    diagram.take_batch(std::numeric_limits<size_t>::max(), uncovered);
    return {uncovered.size(), diagram.node_count()};
}

size_t multithreaded_cbdd_zed_maximum_interval_chain(size_t dimension, uint64_t lower_mask, uint64_t upper_mask)
{
    interval_cbdd diagram(dimension);
    std::vector<subset_result> certificates;
    certificates.emplace_back(dimension);
    certificates.back().ready = true;
    for (size_t bit = 0; bit < dimension; ++bit) {
        if ((lower_mask & (uint64_t{1} << bit)) != 0) certificates.back().lower.set(bit);
        if ((upper_mask & (uint64_t{1} << bit)) != 0) certificates.back().upper.set(bit);
    }
    diagram.add_intervals(certificates, certificates.size());
    return diagram.maximum_chain_length();
}

size_t multithreaded_cbdd_zed_batch_size(size_t dimension, size_t workers)
{
    if (dimension == 0 || workers == 0) throw std::invalid_argument("dimension and workers must be positive");
    return batch_size(dimension, workers);
}

std::vector<uint64_t> multithreaded_cbdd_zed_maximal_block_masks(const matrix_integer& matrix, size_t target)
{
    if (matrix.rows() > 64 || target == 0) throw std::invalid_argument("test helper requires dimension <= 64 and target > 0");
    maximal_z_blocks blocks(matrix);
    const std::vector<z_search_task> tasks = blocks.partition(target);
    const std::atomic<bool> cancelled{false};
    std::vector<uint64_t> result;
    for (const z_search_task& task : tasks) {
        blocks.visit(task, cancelled, [&](const support& block, size_t) {
            uint64_t mask = 0;
            for (size_t index = 0; index < matrix.rows(); ++index)
                if (block.contains(index)) mask |= uint64_t{1} << index;
            result.push_back(mask);
            return true;
        });
    }
    std::sort(result.begin(), result.end());
    return result;
}
#endif

} // namespace coposit::model
