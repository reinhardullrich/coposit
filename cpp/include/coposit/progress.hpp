#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <mutex>
#include <ostream>
#include <sstream>
#include <thread>

namespace coposit::progress {

constexpr auto report_interval = std::chrono::seconds(1);
constexpr uint64_t publish_mask = 4095;
constexpr uint64_t coverage_scale = 1'000'000'000'000ULL;

enum class metric { none, preprocessing, support, simplex, proof, traversal, adaptive };
enum class adaptive_engine { none, routing, sponsel, copomatrix };
enum class adaptive_phase {
    none,
    pivot_scan,
    edge_scan,
    h_build,
    h_factorization,
    split_build,
    copomatrix_partition,
    principal_block,
    schur_block,
    staircase,
    transform,
};
enum class adaptive_route { none, sponsel, narrow_copomatrix, forced_copomatrix };
enum class preprocessing_phase {
    none,
    matrix_scan,
    cheap_certificates,
    principal_submatrices,
    connected_components,
    component_scan,
    frank_wolfe,
    exact_factorization,
    model_delegation,
};

struct snapshot {
    metric kind = metric::none;
    preprocessing_phase phase = preprocessing_phase::none;
    uint64_t nodes = 0;
    uint64_t resolved = 0;
    uint64_t secondary = 0;
    uint64_t splits = 0;
    uint64_t open = 0;
    uint64_t coverage = 0;
    size_t current = 0;
    size_t maximum = 0;
    size_t depth = 0;
    adaptive_engine engine = adaptive_engine::none;
    adaptive_phase model_phase = adaptive_phase::none;
    adaptive_route route = adaptive_route::none;
    uint64_t sponsel_nodes = 0;
    uint64_t sponsel_splits = 0;
    uint64_t copomatrix_nodes = 0;
    uint64_t copomatrix_children = 0;
    uint64_t copomatrix_staircase = 0;
    uint64_t forced_copomatrix = 0;
    uint64_t pivot_children = 0;
    size_t streak = 0;
    size_t pivot = 0;
    size_t work_current = 0;
    size_t work_maximum = 0;
};

namespace detail {

struct shared_state {
    std::atomic<bool> enabled{false};
    std::atomic<metric> kind{metric::none};
    std::atomic<preprocessing_phase> phase{preprocessing_phase::none};
    std::atomic<uint64_t> nodes{0};
    std::atomic<uint64_t> resolved{0};
    std::atomic<uint64_t> secondary{0};
    std::atomic<uint64_t> splits{0};
    std::atomic<uint64_t> open{0};
    std::atomic<uint64_t> coverage{0};
    std::atomic<size_t> current{0};
    std::atomic<size_t> maximum{0};
    std::atomic<size_t> depth{0};
    std::atomic<adaptive_engine> engine{adaptive_engine::none};
    std::atomic<adaptive_phase> model_phase{adaptive_phase::none};
    std::atomic<adaptive_route> route{adaptive_route::none};
    std::atomic<uint64_t> sponsel_nodes{0};
    std::atomic<uint64_t> sponsel_splits{0};
    std::atomic<uint64_t> copomatrix_nodes{0};
    std::atomic<uint64_t> copomatrix_children{0};
    std::atomic<uint64_t> copomatrix_staircase{0};
    std::atomic<uint64_t> forced_copomatrix{0};
    std::atomic<uint64_t> pivot_children{0};
    std::atomic<size_t> streak{0};
    std::atomic<size_t> pivot{0};
    std::atomic<size_t> work_current{0};
    std::atomic<size_t> work_maximum{0};
};

inline shared_state state;

inline void reset() noexcept
{
    state.kind.store(metric::none, std::memory_order_relaxed);
    state.phase.store(preprocessing_phase::none, std::memory_order_relaxed);
    state.nodes.store(0, std::memory_order_relaxed);
    state.resolved.store(0, std::memory_order_relaxed);
    state.secondary.store(0, std::memory_order_relaxed);
    state.splits.store(0, std::memory_order_relaxed);
    state.open.store(0, std::memory_order_relaxed);
    state.coverage.store(0, std::memory_order_relaxed);
    state.current.store(0, std::memory_order_relaxed);
    state.maximum.store(0, std::memory_order_relaxed);
    state.depth.store(0, std::memory_order_relaxed);
    state.engine.store(adaptive_engine::none, std::memory_order_relaxed);
    state.model_phase.store(adaptive_phase::none, std::memory_order_relaxed);
    state.route.store(adaptive_route::none, std::memory_order_relaxed);
    state.sponsel_nodes.store(0, std::memory_order_relaxed);
    state.sponsel_splits.store(0, std::memory_order_relaxed);
    state.copomatrix_nodes.store(0, std::memory_order_relaxed);
    state.copomatrix_children.store(0, std::memory_order_relaxed);
    state.copomatrix_staircase.store(0, std::memory_order_relaxed);
    state.forced_copomatrix.store(0, std::memory_order_relaxed);
    state.pivot_children.store(0, std::memory_order_relaxed);
    state.streak.store(0, std::memory_order_relaxed);
    state.pivot.store(0, std::memory_order_relaxed);
    state.work_current.store(0, std::memory_order_relaxed);
    state.work_maximum.store(0, std::memory_order_relaxed);
}

inline snapshot load() noexcept
{
    return {
        state.kind.load(std::memory_order_relaxed),
        state.phase.load(std::memory_order_relaxed),
        state.nodes.load(std::memory_order_relaxed),
        state.resolved.load(std::memory_order_relaxed),
        state.secondary.load(std::memory_order_relaxed),
        state.splits.load(std::memory_order_relaxed),
        state.open.load(std::memory_order_relaxed),
        state.coverage.load(std::memory_order_relaxed),
        state.current.load(std::memory_order_relaxed),
        state.maximum.load(std::memory_order_relaxed),
        state.depth.load(std::memory_order_relaxed),
        state.engine.load(std::memory_order_relaxed),
        state.model_phase.load(std::memory_order_relaxed),
        state.route.load(std::memory_order_relaxed),
        state.sponsel_nodes.load(std::memory_order_relaxed),
        state.sponsel_splits.load(std::memory_order_relaxed),
        state.copomatrix_nodes.load(std::memory_order_relaxed),
        state.copomatrix_children.load(std::memory_order_relaxed),
        state.copomatrix_staircase.load(std::memory_order_relaxed),
        state.forced_copomatrix.load(std::memory_order_relaxed),
        state.pivot_children.load(std::memory_order_relaxed),
        state.streak.load(std::memory_order_relaxed),
        state.pivot.load(std::memory_order_relaxed),
        state.work_current.load(std::memory_order_relaxed),
        state.work_maximum.load(std::memory_order_relaxed),
    };
}

inline const char* engine_text(adaptive_engine engine) noexcept
{
    switch (engine) {
        case adaptive_engine::routing: return "routing";
        case adaptive_engine::sponsel: return "sponsel";
        case adaptive_engine::copomatrix: return "copomatrix";
        case adaptive_engine::none: return "starting";
    }
    return "starting";
}

inline const char* model_phase_text(adaptive_phase phase) noexcept
{
    switch (phase) {
        case adaptive_phase::pivot_scan: return "pivot scan";
        case adaptive_phase::edge_scan: return "edge scan";
        case adaptive_phase::h_build: return "H build";
        case adaptive_phase::h_factorization: return "H factorization";
        case adaptive_phase::split_build: return "split build";
        case adaptive_phase::copomatrix_partition: return "partition";
        case adaptive_phase::principal_block: return "principal block";
        case adaptive_phase::schur_block: return "Schur block";
        case adaptive_phase::staircase: return "staircase";
        case adaptive_phase::transform: return "transform";
        case adaptive_phase::none: return "starting";
    }
    return "starting";
}

inline const char* route_text(adaptive_route route) noexcept
{
    switch (route) {
        case adaptive_route::sponsel: return "wide-to-sponsel";
        case adaptive_route::narrow_copomatrix: return "narrow-to-copomatrix";
        case adaptive_route::forced_copomatrix: return "cutoff-to-copomatrix";
        case adaptive_route::none: return "undecided";
    }
    return "undecided";
}

inline const char* phase_text(preprocessing_phase phase) noexcept
{
    switch (phase) {
        case preprocessing_phase::matrix_scan:
            return "matrix scan";
        case preprocessing_phase::cheap_certificates:
            return "cheap certificates";
        case preprocessing_phase::principal_submatrices:
            return "principal submatrices";
        case preprocessing_phase::connected_components:
            return "connected components";
        case preprocessing_phase::component_scan:
            return "component scan";
        case preprocessing_phase::frank_wolfe:
            return "Frank-Wolfe";
        case preprocessing_phase::exact_factorization:
            return "exact factorization";
        case preprocessing_phase::model_delegation:
            return "model delegation";
        case preprocessing_phase::none:
            return "starting";
    }
    return "starting";
}

inline void increment(uint64_t& value) noexcept
{
    // ponytail: 64-bit telemetry saturates; widen only if a real run can visit 2^64 nodes.
    if (value != std::numeric_limits<uint64_t>::max()) ++value;
}

inline long double support_percent(uint64_t visited, size_t dimension) noexcept
{
    if (dimension == 0) return 100.0L;
    if (dimension >= static_cast<size_t>(std::numeric_limits<long double>::max_exponent)) return 0.0L;
    const long double total = std::ldexp(1.0L, static_cast<int>(dimension)) - 1.0L;
    return 100.0L * static_cast<long double>(visited) / total;
}

inline std::string elapsed_text(std::chrono::seconds elapsed)
{
    const auto total = elapsed.count();
    const auto hours = total / 3600;
    const auto minutes = total / 60 % 60;
    const auto seconds = total % 60;
    std::ostringstream text;
    text << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes << ':' << std::setw(2) << seconds;
    return text.str();
}

inline std::string format(const snapshot& value, std::chrono::seconds elapsed, double rate)
{
    std::ostringstream output;
    output << '[' << elapsed_text(elapsed) << "] ";
    output << std::fixed;
    switch (value.kind) {
        case metric::preprocessing:
            output << "stage=preprocessing  phase=" << phase_text(value.phase);
            if (value.maximum != 0) output << "  work=" << value.current << '/' << value.maximum;
            else if (value.current != 0) output << "  item=" << value.current;
            if (value.depth != 0) output << "  dimension=" << value.depth;
            break;
        case metric::support:
            output << "stage=model  metric=support  coverage=" << std::setprecision(6)
                   << support_percent(value.nodes, value.maximum) << "%"
                   << "  cardinality=" << value.current << '/' << value.maximum << "  visited=" << value.nodes
                   << "  covered=" << value.resolved << "  processed=" << value.secondary;
            break;
        case metric::simplex:
            output << "stage=model  metric=simplex  coverage=" << std::setprecision(3)
                   << 100.0L * static_cast<long double>(value.coverage) / coverage_scale << "%"
                   << "  nodes=" << value.nodes << "  certified=" << value.resolved << "  splits=" << value.splits
                   << "  open=" << value.open << "  depth=" << value.depth;
            break;
        case metric::proof:
            output << "stage=model  metric=proof  coverage=" << std::setprecision(3)
                   << 100.0L * static_cast<long double>(value.coverage) / coverage_scale << "%"
                   << "  nodes=" << value.nodes << "  certified=" << value.resolved << "  splits=" << value.splits
                   << "  dimension=" << value.current << '/' << value.maximum << "  depth=" << value.depth;
            break;
        case metric::traversal:
            output << "stage=model  metric=traversal  nodes=" << value.nodes << "  certified=" << value.resolved
                   << "  splits=" << value.splits
                   << "  open=" << value.open << "  dimension=" << value.current << '/' << value.maximum
                   << "  depth=" << value.depth;
            break;
        case metric::adaptive:
            output << "stage=model  metric=adaptive  engine=" << engine_text(value.engine)
                   << "  phase=" << model_phase_text(value.model_phase) << "  route=" << route_text(value.route)
                   << "  coverage=" << std::setprecision(3)
                   << 100.0L * static_cast<long double>(value.coverage) / coverage_scale << "%"
                   << "  nodes=" << value.nodes << "  certified=" << value.resolved
                   << "  sponsel_nodes=" << value.sponsel_nodes << "  sponsel_splits=" << value.sponsel_splits
                   << "  copomatrix_nodes=" << value.copomatrix_nodes
                   << "  copomatrix_children=" << value.copomatrix_children
                   << "  staircase=" << value.copomatrix_staircase
                   << "  forced_copomatrix=" << value.forced_copomatrix << "  streak=" << value.streak
                   << "  dimension=" << value.current << '/' << value.maximum << "  depth=" << value.depth;
            if (value.pivot_children != 0) {
                output << "  pivot=" << value.pivot + 1 << "  pivot_children=";
                if (value.pivot_children == std::numeric_limits<uint64_t>::max()) output << ">=";
                output << value.pivot_children;
            }
            if (value.work_maximum != 0) output << "  work=" << value.work_current << '/' << value.work_maximum;
            break;
        case metric::none:
            output << "stage=starting  waiting for progress source";
            break;
    }
    if (value.kind != metric::none && value.kind != metric::preprocessing) {
        output << "  rate=" << std::setprecision(1) << rate << "/s";
    }
    return output.str();
}

} // namespace detail

inline bool enabled() noexcept
{
    return detail::state.enabled.load(std::memory_order_relaxed);
}

inline void preprocessing_stage(preprocessing_phase phase, size_t dimension, size_t current = 0, size_t maximum = 0) noexcept
{
    if (!enabled()) return;
    detail::state.kind.store(metric::none, std::memory_order_relaxed);
    detail::state.phase.store(phase, std::memory_order_relaxed);
    detail::state.current.store(current, std::memory_order_relaxed);
    detail::state.maximum.store(maximum, std::memory_order_relaxed);
    detail::state.depth.store(dimension, std::memory_order_relaxed);
    detail::state.kind.store(metric::preprocessing, std::memory_order_relaxed);
}

inline void advance_preprocessing(size_t current, size_t maximum) noexcept
{
    if (!enabled() || detail::state.kind.load(std::memory_order_relaxed) != metric::preprocessing) return;
    detail::state.current.store(current, std::memory_order_relaxed);
    detail::state.maximum.store(maximum, std::memory_order_relaxed);
}

class tracker {
public:
    tracker(metric kind, size_t maximum) noexcept : active_(enabled()), kind_(kind), maximum_(maximum), current_(maximum)
    {
        if (!active_) return;
        detail::reset();
        detail::state.maximum.store(maximum_, std::memory_order_relaxed);
        detail::state.current.store(current_, std::memory_order_relaxed);
        detail::state.kind.store(kind_, std::memory_order_relaxed);
    }

    bool active() const noexcept { return active_; }

    void stage(size_t current, size_t depth = 0, size_t open = 0) noexcept
    {
        if (!active_) return;
        current_ = current;
        depth_ = depth;
        open_ = open;
        publish();
    }

    void visit(size_t current, size_t depth, size_t open = 0) noexcept
    {
        if (!active_) return;
        detail::increment(nodes_);
        current_ = current;
        depth_ = depth;
        open_ = open;
        if (nodes_ == 1 || (nodes_ & publish_mask) == 0) publish();
    }

    void visit_support() noexcept
    {
        if (!active_) return;
        detail::increment(nodes_);
        if (nodes_ == 1 || (nodes_ & publish_mask) == 0) publish();
    }

    void covered_support() noexcept
    {
        if (active_) detail::increment(resolved_);
    }

    void resolved(long double weight = 0.0L) noexcept
    {
        if (!active_) return;
        detail::increment(resolved_);
        if (weight != 0.0L) coverage_ = std::min(1.0L, coverage_ + weight);
    }

    void secondary() noexcept
    {
        if (active_) detail::increment(secondary_);
    }

    void split() noexcept
    {
        if (active_) detail::increment(splits_);
    }

    void finish() noexcept
    {
        if (active_) publish();
    }

    void adaptive_stage(adaptive_engine engine, adaptive_phase phase, size_t work_current = 0,
                        size_t work_maximum = 0) noexcept
    {
        if (!active_) return;
        engine_ = engine;
        model_phase_ = phase;
        work_current_ = work_current;
        work_maximum_ = work_maximum;
        publish();
    }

    void adaptive_routing(size_t streak) noexcept
    {
        if (!active_) return;
        engine_ = adaptive_engine::routing;
        model_phase_ = adaptive_phase::pivot_scan;
        route_ = adaptive_route::none;
        streak_ = streak;
        pivot_children_ = 0;
        work_current_ = 0;
        work_maximum_ = 0;
        publish();
    }

    void adaptive_work(size_t current, size_t maximum) noexcept
    {
        if (!active_) return;
        work_current_ = current;
        work_maximum_ = maximum;
        detail::state.work_current.store(current, std::memory_order_relaxed);
        detail::state.work_maximum.store(maximum, std::memory_order_relaxed);
    }

    void adaptive_sponsel(size_t streak, size_t pivot, uint64_t pivot_children) noexcept
    {
        if (!active_) return;
        detail::increment(sponsel_nodes_);
        engine_ = adaptive_engine::sponsel;
        route_ = adaptive_route::sponsel;
        streak_ = streak;
        pivot_ = pivot;
        pivot_children_ = pivot_children;
        publish();
    }

    void adaptive_sponsel_split() noexcept
    {
        if (!active_) return;
        detail::increment(sponsel_splits_);
        if (sponsel_splits_ == 1 || (sponsel_splits_ & publish_mask) == 0) publish();
    }

    void adaptive_copomatrix(size_t streak, size_t pivot, uint64_t pivot_children, bool forced) noexcept
    {
        if (!active_) return;
        detail::increment(copomatrix_nodes_);
        if (forced) detail::increment(forced_copomatrix_);
        engine_ = adaptive_engine::copomatrix;
        route_ = forced ? adaptive_route::forced_copomatrix : adaptive_route::narrow_copomatrix;
        streak_ = streak;
        pivot_ = pivot;
        pivot_children_ = pivot_children;
        publish();
    }

    void adaptive_copomatrix_child() noexcept
    {
        if (!active_) return;
        detail::increment(copomatrix_children_);
        if (copomatrix_children_ == 1 || (copomatrix_children_ & publish_mask) == 0) publish();
    }

    void adaptive_copomatrix_staircase() noexcept
    {
        if (!active_) return;
        detail::increment(copomatrix_staircase_);
        engine_ = adaptive_engine::copomatrix;
        model_phase_ = adaptive_phase::staircase;
        if (copomatrix_staircase_ == 1 || (copomatrix_staircase_ & publish_mask) == 0) publish();
    }

private:
    void publish() const noexcept
    {
        detail::state.nodes.store(nodes_, std::memory_order_relaxed);
        detail::state.resolved.store(resolved_, std::memory_order_relaxed);
        detail::state.secondary.store(secondary_, std::memory_order_relaxed);
        detail::state.splits.store(splits_, std::memory_order_relaxed);
        detail::state.open.store(open_, std::memory_order_relaxed);
        detail::state.coverage.store(static_cast<uint64_t>(std::clamp(coverage_, 0.0L, 1.0L) * coverage_scale),
                                     std::memory_order_relaxed);
        detail::state.current.store(current_, std::memory_order_relaxed);
        detail::state.depth.store(depth_, std::memory_order_relaxed);
        detail::state.engine.store(engine_, std::memory_order_relaxed);
        detail::state.model_phase.store(model_phase_, std::memory_order_relaxed);
        detail::state.route.store(route_, std::memory_order_relaxed);
        detail::state.sponsel_nodes.store(sponsel_nodes_, std::memory_order_relaxed);
        detail::state.sponsel_splits.store(sponsel_splits_, std::memory_order_relaxed);
        detail::state.copomatrix_nodes.store(copomatrix_nodes_, std::memory_order_relaxed);
        detail::state.copomatrix_children.store(copomatrix_children_, std::memory_order_relaxed);
        detail::state.copomatrix_staircase.store(copomatrix_staircase_, std::memory_order_relaxed);
        detail::state.forced_copomatrix.store(forced_copomatrix_, std::memory_order_relaxed);
        detail::state.pivot_children.store(pivot_children_, std::memory_order_relaxed);
        detail::state.streak.store(streak_, std::memory_order_relaxed);
        detail::state.pivot.store(pivot_, std::memory_order_relaxed);
        detail::state.work_current.store(work_current_, std::memory_order_relaxed);
        detail::state.work_maximum.store(work_maximum_, std::memory_order_relaxed);
    }

    const bool active_;
    const metric kind_;
    const size_t maximum_;
    uint64_t nodes_ = 0;
    uint64_t resolved_ = 0;
    uint64_t secondary_ = 0;
    uint64_t splits_ = 0;
    size_t current_;
    size_t depth_ = 0;
    size_t open_ = 0;
    long double coverage_ = 0.0L;
    adaptive_engine engine_ = adaptive_engine::none;
    adaptive_phase model_phase_ = adaptive_phase::none;
    adaptive_route route_ = adaptive_route::none;
    uint64_t sponsel_nodes_ = 0;
    uint64_t sponsel_splits_ = 0;
    uint64_t copomatrix_nodes_ = 0;
    uint64_t copomatrix_children_ = 0;
    uint64_t copomatrix_staircase_ = 0;
    uint64_t forced_copomatrix_ = 0;
    uint64_t pivot_children_ = 0;
    size_t streak_ = 0;
    size_t pivot_ = 0;
    size_t work_current_ = 0;
    size_t work_maximum_ = 0;
};

class reporter {
public:
    reporter(bool active, std::ostream& output) : active_(active), output_(output)
    {
        if (!active_) return;
        detail::reset();
        detail::state.enabled.store(true, std::memory_order_relaxed);
        started_ = std::chrono::steady_clock::now();
        thread_ = std::thread([this] { run(); });
    }

    ~reporter() { stop(); }

    reporter(const reporter&) = delete;
    reporter& operator=(const reporter&) = delete;

    void stop() noexcept
    {
        if (!active_) return;
        detail::state.enabled.store(false, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        condition_.notify_one();
        if (thread_.joinable()) thread_.join();
        active_ = false;
    }

private:
    void run()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!condition_.wait_for(lock, report_interval, [this] { return stopped_; })) {
            lock.unlock();
            const auto now = std::chrono::steady_clock::now();
            const snapshot current = detail::load();
            const double seconds = std::chrono::duration<double>(now - last_report_).count();
            const double rate = current.nodes >= last_nodes_ && seconds > 0.0
                ? static_cast<double>(current.nodes - last_nodes_) / seconds
                : 0.0;
            output_ << detail::format(current, std::chrono::duration_cast<std::chrono::seconds>(now - started_), rate) << '\n'
                    << std::flush;
            last_report_ = now;
            last_nodes_ = current.nodes;
            lock.lock();
        }
    }

    bool active_;
    std::ostream& output_;
    std::chrono::steady_clock::time_point started_{};
    std::chrono::steady_clock::time_point last_report_ = std::chrono::steady_clock::now();
    uint64_t last_nodes_ = 0;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopped_ = false;
    std::thread thread_;
};

} // namespace coposit::progress
