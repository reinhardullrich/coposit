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
#include <map>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>

namespace coposit::diagnostics {

constexpr auto report_interval = std::chrono::seconds(1);
constexpr uint64_t publish_mask = 4095;
constexpr uint64_t bracelet_publish_mask = 255;
constexpr uint64_t decision_diagram_publish_interval = 200;
constexpr uint64_t coverage_scale = 1'000'000'000'000ULL;

enum class metric { none, preprocessing, support, decision_diagram, bracelet, simplex, proof, traversal, adaptive };
enum class decision_diagram_phase { none, cardinality_build, support_solve, certificate_union, certificate_subtract };
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
    root_checks,
    cheap_certificates,
    principal_submatrices,
    negative_part_diagonal_dominance,
    all_ones,
    connected_components,
    component_scan,
    motzkin_straus,
    z_matrix,
    frank_wolfe,
    exact_factorization,
    danninger,
    copomatrix,
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
    uint64_t lifted_processed = 0;
    uint64_t lift_duplicate_skips = 0;
    uint64_t lift_covered_skips = 0;
    size_t lift_dimension = 0;
    size_t lift_depth = 0;
    size_t lift_maximum_dimension = 0;
    size_t lift_maximum_depth = 0;
    size_t lift_cache_size = 0;
    size_t lift_frontier_size = 0;
    size_t lift_maximum_frontier_size = 0;
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
    decision_diagram_phase diagram_phase = decision_diagram_phase::none;
    int64_t cardinality_started_ns = 0;
    size_t preprocessing_root_dimension = 0;
    bool preprocessing_finished = false;
    bool preprocessing_component_split = false;
    size_t preprocessing_components_seen = 0;
    size_t preprocessing_largest_component = 0;
    size_t preprocessing_pending_components = 0;
    size_t preprocessing_largest_pending_component = 0;
    size_t preprocessing_reduction_child_checks = 0;
    size_t preprocessing_maximum_reduction_depth = 0;
    size_t preprocessing_reduction_decisions = 0;
    size_t preprocessing_model_delegations = 0;
    std::map<std::pair<size_t, size_t>, uint64_t> certificate_cardinality_free_index_counts;
    std::map<std::tuple<size_t, size_t, size_t>, uint64_t> certificate_cardinality_free_index_upper_size_counts;
    std::map<std::tuple<size_t, size_t, size_t, size_t>, uint64_t> certificate_root_lifted_upper_lower_counts;
    std::map<std::pair<size_t, size_t>, uint64_t> singular_cardinality_nullity_counts;
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
    std::atomic<uint64_t> lifted_processed{0};
    std::atomic<uint64_t> lift_duplicate_skips{0};
    std::atomic<uint64_t> lift_covered_skips{0};
    std::atomic<size_t> lift_dimension{0};
    std::atomic<size_t> lift_depth{0};
    std::atomic<size_t> lift_maximum_dimension{0};
    std::atomic<size_t> lift_maximum_depth{0};
    std::atomic<size_t> lift_cache_size{0};
    std::atomic<size_t> lift_frontier_size{0};
    std::atomic<size_t> lift_maximum_frontier_size{0};
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
    std::atomic<decision_diagram_phase> diagram_phase{decision_diagram_phase::none};
    std::atomic<int64_t> cardinality_started_ns{0};
    std::atomic<size_t> preprocessing_root_dimension{0};
    std::atomic<bool> preprocessing_finished{false};
    std::atomic<bool> preprocessing_component_split{false};
    std::atomic<size_t> preprocessing_components_seen{0};
    std::atomic<size_t> preprocessing_largest_component{0};
    std::atomic<size_t> preprocessing_pending_components{0};
    std::atomic<size_t> preprocessing_largest_pending_component{0};
    std::atomic<size_t> preprocessing_reduction_child_checks{0};
    std::atomic<size_t> preprocessing_maximum_reduction_depth{0};
    std::atomic<size_t> preprocessing_reduction_decisions{0};
    std::atomic<size_t> preprocessing_model_delegations{0};
    std::mutex certificate_histogram_mutex;
    std::map<std::pair<size_t, size_t>, uint64_t> certificate_cardinality_free_index_counts;
    std::map<std::tuple<size_t, size_t, size_t>, uint64_t> certificate_cardinality_free_index_upper_size_counts;
    std::map<std::tuple<size_t, size_t, size_t, size_t>, uint64_t> certificate_root_lifted_upper_lower_counts;
    std::map<std::pair<size_t, size_t>, uint64_t> singular_cardinality_nullity_counts;
    std::mutex diagnostics_mutex;
    std::string diagnostics;
};

inline shared_state state;

inline void reset_current_metric() noexcept
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
    state.lifted_processed.store(0, std::memory_order_relaxed);
    state.lift_duplicate_skips.store(0, std::memory_order_relaxed);
    state.lift_covered_skips.store(0, std::memory_order_relaxed);
    state.lift_dimension.store(0, std::memory_order_relaxed);
    state.lift_depth.store(0, std::memory_order_relaxed);
    state.lift_maximum_dimension.store(0, std::memory_order_relaxed);
    state.lift_maximum_depth.store(0, std::memory_order_relaxed);
    state.lift_cache_size.store(0, std::memory_order_relaxed);
    state.lift_frontier_size.store(0, std::memory_order_relaxed);
    state.lift_maximum_frontier_size.store(0, std::memory_order_relaxed);
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
    state.diagram_phase.store(decision_diagram_phase::none, std::memory_order_relaxed);
    state.cardinality_started_ns.store(0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(state.certificate_histogram_mutex);
        state.certificate_cardinality_free_index_counts.clear();
        state.certificate_cardinality_free_index_upper_size_counts.clear();
        state.certificate_root_lifted_upper_lower_counts.clear();
        state.singular_cardinality_nullity_counts.clear();
    }
}

inline void reset() noexcept
{
    reset_current_metric();
    state.preprocessing_root_dimension.store(0, std::memory_order_relaxed);
    state.preprocessing_finished.store(false, std::memory_order_relaxed);
    state.preprocessing_component_split.store(false, std::memory_order_relaxed);
    state.preprocessing_components_seen.store(0, std::memory_order_relaxed);
    state.preprocessing_largest_component.store(0, std::memory_order_relaxed);
    state.preprocessing_pending_components.store(0, std::memory_order_relaxed);
    state.preprocessing_largest_pending_component.store(0, std::memory_order_relaxed);
    state.preprocessing_reduction_child_checks.store(0, std::memory_order_relaxed);
    state.preprocessing_maximum_reduction_depth.store(0, std::memory_order_relaxed);
    state.preprocessing_reduction_decisions.store(0, std::memory_order_relaxed);
    state.preprocessing_model_delegations.store(0, std::memory_order_relaxed);
    std::lock_guard<std::mutex> diagnostics_lock(state.diagnostics_mutex);
    state.diagnostics.clear();
}

inline snapshot load() noexcept
{
    snapshot result{
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
        state.lifted_processed.load(std::memory_order_relaxed),
        state.lift_duplicate_skips.load(std::memory_order_relaxed),
        state.lift_covered_skips.load(std::memory_order_relaxed),
        state.lift_dimension.load(std::memory_order_relaxed),
        state.lift_depth.load(std::memory_order_relaxed),
        state.lift_maximum_dimension.load(std::memory_order_relaxed),
        state.lift_maximum_depth.load(std::memory_order_relaxed),
        state.lift_cache_size.load(std::memory_order_relaxed),
        state.lift_frontier_size.load(std::memory_order_relaxed),
        state.lift_maximum_frontier_size.load(std::memory_order_relaxed),
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
        state.diagram_phase.load(std::memory_order_relaxed),
        state.cardinality_started_ns.load(std::memory_order_relaxed),
        state.preprocessing_root_dimension.load(std::memory_order_relaxed),
        state.preprocessing_finished.load(std::memory_order_relaxed),
        state.preprocessing_component_split.load(std::memory_order_relaxed),
        state.preprocessing_components_seen.load(std::memory_order_relaxed),
        state.preprocessing_largest_component.load(std::memory_order_relaxed),
        state.preprocessing_pending_components.load(std::memory_order_relaxed),
        state.preprocessing_largest_pending_component.load(std::memory_order_relaxed),
        state.preprocessing_reduction_child_checks.load(std::memory_order_relaxed),
        state.preprocessing_maximum_reduction_depth.load(std::memory_order_relaxed),
        state.preprocessing_reduction_decisions.load(std::memory_order_relaxed),
        state.preprocessing_model_delegations.load(std::memory_order_relaxed),
    };
    {
        std::lock_guard<std::mutex> lock(state.certificate_histogram_mutex);
        result.certificate_cardinality_free_index_counts = state.certificate_cardinality_free_index_counts;
        result.certificate_cardinality_free_index_upper_size_counts = state.certificate_cardinality_free_index_upper_size_counts;
        result.certificate_root_lifted_upper_lower_counts = state.certificate_root_lifted_upper_lower_counts;
        result.singular_cardinality_nullity_counts = state.singular_cardinality_nullity_counts;
    }
    return result;
}

inline std::string load_diagnostics()
{
    std::lock_guard<std::mutex> lock(state.diagnostics_mutex);
    return state.diagnostics;
}

inline const char* diagram_phase_text(decision_diagram_phase phase) noexcept
{
    switch (phase) {
    case decision_diagram_phase::cardinality_build:
        return "cardinality build";
    case decision_diagram_phase::support_solve:
        return "support solve";
    case decision_diagram_phase::certificate_union:
        return "certificate union";
    case decision_diagram_phase::certificate_subtract:
        return "certificate subtract";
    case decision_diagram_phase::none:
        return "starting";
    }
    return "starting";
}

inline const char* engine_text(adaptive_engine engine) noexcept
{
    switch (engine) {
    case adaptive_engine::routing:
        return "routing";
    case adaptive_engine::sponsel:
        return "sponsel";
    case adaptive_engine::copomatrix:
        return "copomatrix";
    case adaptive_engine::none:
        return "starting";
    }
    return "starting";
}

inline const char* model_phase_text(adaptive_phase phase) noexcept
{
    switch (phase) {
    case adaptive_phase::pivot_scan:
        return "pivot scan";
    case adaptive_phase::edge_scan:
        return "edge scan";
    case adaptive_phase::h_build:
        return "H build";
    case adaptive_phase::h_factorization:
        return "H factorization";
    case adaptive_phase::split_build:
        return "split build";
    case adaptive_phase::copomatrix_partition:
        return "partition";
    case adaptive_phase::principal_block:
        return "principal block";
    case adaptive_phase::schur_block:
        return "Schur block";
    case adaptive_phase::staircase:
        return "staircase";
    case adaptive_phase::transform:
        return "transform";
    case adaptive_phase::none:
        return "starting";
    }
    return "starting";
}

inline const char* route_text(adaptive_route route) noexcept
{
    switch (route) {
    case adaptive_route::sponsel:
        return "wide-to-sponsel";
    case adaptive_route::narrow_copomatrix:
        return "narrow-to-copomatrix";
    case adaptive_route::forced_copomatrix:
        return "cutoff-to-copomatrix";
    case adaptive_route::none:
        return "undecided";
    }
    return "undecided";
}

inline const char* phase_text(preprocessing_phase phase) noexcept
{
    switch (phase) {
    case preprocessing_phase::matrix_scan:
        return "matrix scan";
    case preprocessing_phase::root_checks:
        return "root checks";
    case preprocessing_phase::cheap_certificates:
        return "cheap certificates";
    case preprocessing_phase::principal_submatrices:
        return "principal submatrices";
    case preprocessing_phase::negative_part_diagonal_dominance:
        return "negative-part diagonal dominance";
    case preprocessing_phase::all_ones:
        return "all-ones";
    case preprocessing_phase::connected_components:
        return "connected components";
    case preprocessing_phase::component_scan:
        return "component scan";
    case preprocessing_phase::motzkin_straus:
        return "Motzkin-Straus";
    case preprocessing_phase::z_matrix:
        return "Z-matrix";
    case preprocessing_phase::frank_wolfe:
        return "Frank-Wolfe";
    case preprocessing_phase::exact_factorization:
        return "exact factorization";
    case preprocessing_phase::danninger:
        return "Danninger";
    case preprocessing_phase::copomatrix:
        return "COPOMATRIX";
    case preprocessing_phase::model_delegation:
        return "model delegation";
    case preprocessing_phase::none:
        return "starting";
    }
    return "starting";
}

inline void increment(uint64_t& value) noexcept
{
    // ponytail: 64-bit telemetry saturates; widen only if a real run can visit
    // 2^64 nodes.
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

inline std::string format(const snapshot& value, std::chrono::seconds elapsed, double rate,
                          std::chrono::seconds cardinality_elapsed = std::chrono::seconds(0))
{
    std::ostringstream output;
    output << '[' << elapsed_text(elapsed) << "] ";
    output << std::fixed;
    switch (value.kind) {
    case metric::preprocessing:
        output << "stage=preprocessing  phase=" << phase_text(value.phase);
        if (value.maximum != 0)
            output << "  work=" << value.current << '/' << value.maximum;
        else if (value.current != 0)
            output << "  item=" << value.current;
        if (value.depth != 0) output << "  dimension=" << value.depth;
        break;
    case metric::support:
        output << "stage=model  metric=support  coverage=" << std::setprecision(6) << support_percent(value.nodes, value.maximum) << "%"
               << "  cardinality=" << value.current << '/' << value.maximum << "  visited=" << value.nodes << "  covered=" << value.resolved
               << "  processed=" << value.secondary << "  certificates=" << value.splits;
        if (value.lifted_processed != 0 || value.lift_duplicate_skips != 0 || value.lift_covered_skips != 0 || value.lift_dimension != 0) {
            output << "  outer_processed=" << value.secondary - value.lifted_processed
                   << "  lifted_processed=" << value.lifted_processed << "  lift_duplicate_skips=" << value.lift_duplicate_skips
                   << "  lift_covered_skips=" << value.lift_covered_skips << "  lift_cache_size=" << value.lift_cache_size;
            if (value.lift_dimension != 0)
                output << "  lift_dimension=" << value.lift_dimension << "  lift_depth=" << value.lift_depth;
            output << "  lift_maximum_dimension=" << value.lift_maximum_dimension << "  lift_maximum_depth=" << value.lift_maximum_depth;
            if (value.lift_frontier_size != 0 || value.lift_maximum_frontier_size != 0)
                output << "  lift_frontier=" << value.lift_frontier_size << "  lift_maximum_frontier=" << value.lift_maximum_frontier_size;
        }
        if (!value.certificate_root_lifted_upper_lower_counts.empty()) {
            output << "  certificate_root_k_lifted_k_u_l_counts=[";
            bool first = true;
            for (const auto& [key, count] : value.certificate_root_lifted_upper_lower_counts) {
                if (!first) output << ',';
                const auto& [root_cardinality, lifted_cardinality, upper_size, lower_size] = key;
                output << '(' << root_cardinality << ',' << lifted_cardinality << ',' << upper_size << ',' << lower_size << ',' << count
                       << ')';
                first = false;
            }
            output << ']';
        } else if (!value.certificate_cardinality_free_index_upper_size_counts.empty()) {
            output << "  certificate_k_d_u_counts=[";
            bool first = true;
            for (const auto& [key, count] : value.certificate_cardinality_free_index_upper_size_counts) {
                if (!first) output << ',';
                const auto& [cardinality, free_indices, upper_size] = key;
                output << '(' << cardinality << ',' << free_indices << ',' << upper_size << ',' << count << ')';
                first = false;
            }
            output << ']';
        } else if (!value.certificate_cardinality_free_index_counts.empty()) {
            output << "  certificate_k_d_counts=[";
            bool first = true;
            for (const auto& [key, count] : value.certificate_cardinality_free_index_counts) {
                if (!first) output << ',';
                output << '(' << key.first << ',' << key.second << ',' << count << ')';
                first = false;
            }
            output << ']';
        }
        if (!value.singular_cardinality_nullity_counts.empty()) {
            output << "  singular_k_q_counts=[";
            bool first = true;
            for (const auto& [key, count] : value.singular_cardinality_nullity_counts) {
                if (!first) output << ',';
                output << '(' << key.first << ',' << key.second << ',' << count << ')';
                first = false;
            }
            output << ']';
        }
        break;
    case metric::decision_diagram:
        output << "stage=model  metric=decision-diagram  phase=" << diagram_phase_text(value.diagram_phase)
               << "  cardinality=" << value.current << '/' << value.maximum << "  ";
        if (value.current == 0)
            output << "phase_elapsed=";
        else
            output << "cardinality_elapsed=";
        output << elapsed_text(cardinality_elapsed) << "  emitted_supports=" << value.nodes << "  certificates=" << value.resolved
               << "  dd_nodes_allocated=" << value.secondary << "  dd_operations=" << value.splits;
        if (!value.certificate_cardinality_free_index_upper_size_counts.empty()) {
            output << "  certificate_k_d_u_counts=[";
            bool first = true;
            for (const auto& [key, count] : value.certificate_cardinality_free_index_upper_size_counts) {
                if (!first) output << ',';
                const auto& [cardinality, free_indices, upper_size] = key;
                output << '(' << cardinality << ',' << free_indices << ',' << upper_size << ',' << count << ')';
                first = false;
            }
            output << ']';
        } else if (!value.certificate_cardinality_free_index_counts.empty()) {
            output << "  certificate_k_d_counts=[";
            bool first = true;
            for (const auto& [key, count] : value.certificate_cardinality_free_index_counts) {
                if (!first) output << ',';
                output << '(' << key.first << ',' << key.second << ',' << count << ')';
                first = false;
            }
            output << ']';
        }
        break;
    case metric::bracelet:
        output << "stage=model  metric=bracelet  cardinality=" << value.current << '/' << value.maximum << "  bracelets=" << value.nodes
               << "  affine_skipped=" << value.resolved << "  exact_systems=" << value.secondary << "  candidates=" << value.splits;
        break;
    case metric::simplex:
        output << "stage=model  metric=simplex  coverage=" << std::setprecision(3)
               << 100.0L * static_cast<long double>(value.coverage) / coverage_scale << "%"
               << "  nodes=" << value.nodes << "  certified=" << value.resolved << "  splits=" << value.splits << "  open=" << value.open
               << "  depth=" << value.depth;
        break;
    case metric::proof:
        output << "stage=model  metric=proof  coverage=" << std::setprecision(3)
               << 100.0L * static_cast<long double>(value.coverage) / coverage_scale << "%"
               << "  nodes=" << value.nodes << "  certified=" << value.resolved << "  splits=" << value.splits
               << "  dimension=" << value.current << '/' << value.maximum << "  depth=" << value.depth;
        break;
    case metric::traversal:
        output << "stage=model  metric=traversal  nodes=" << value.nodes << "  certified=" << value.resolved << "  splits=" << value.splits
               << "  open=" << value.open << "  dimension=" << value.current << '/' << value.maximum << "  depth=" << value.depth;
        break;
    case metric::adaptive:
        output << "stage=model  metric=adaptive  engine=" << engine_text(value.engine) << "  phase=" << model_phase_text(value.model_phase)
               << "  route=" << route_text(value.route) << "  coverage=" << std::setprecision(3)
               << 100.0L * static_cast<long double>(value.coverage) / coverage_scale << "%"
               << "  nodes=" << value.nodes << "  certified=" << value.resolved << "  sponsel_nodes=" << value.sponsel_nodes
               << "  sponsel_splits=" << value.sponsel_splits << "  copomatrix_nodes=" << value.copomatrix_nodes
               << "  copomatrix_children=" << value.copomatrix_children << "  staircase=" << value.copomatrix_staircase
               << "  forced_copomatrix=" << value.forced_copomatrix << "  streak=" << value.streak << "  dimension=" << value.current << '/'
               << value.maximum << "  depth=" << value.depth;
        if (value.pivot_children != 0) {
            output << "  pivot=" << value.pivot + 1 << "  pivot_children=";
            if (value.pivot_children == std::numeric_limits<uint64_t>::max()) output << ">=";
            output << value.pivot_children;
        }
        if (value.work_maximum != 0) output << "  work=" << value.work_current << '/' << value.work_maximum;
        break;
    case metric::none:
        output << "stage=starting  waiting for diagnostics source";
        break;
    }
    if (value.preprocessing_root_dimension != 0) {
        const char* preprocessing_outcome = !value.preprocessing_finished                 ? "running"
                                            : value.preprocessing_pending_components == 0 ? "resolved"
                                                                                          : "pending";
        output << "  preprocessing_root=" << value.preprocessing_root_dimension << "  preprocessing_outcome=" << preprocessing_outcome
               << "  component_split=" << (value.preprocessing_component_split ? "yes" : "no")
               << "  components_seen=" << value.preprocessing_components_seen
               << "  largest_component=" << value.preprocessing_largest_component
               << "  pending_components=" << value.preprocessing_pending_components
               << "  largest_pending_component=" << value.preprocessing_largest_pending_component
               << "  reduction_child_checks=" << value.preprocessing_reduction_child_checks
               << "  maximum_reduction_depth=" << value.preprocessing_maximum_reduction_depth
               << "  reduction_decisions=" << value.preprocessing_reduction_decisions
               << "  model_delegations=" << value.preprocessing_model_delegations;
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

inline void preprocessing_begin(size_t root_dimension) noexcept
{
    if (!enabled()) return;
    detail::state.preprocessing_root_dimension.store(root_dimension, std::memory_order_relaxed);
    detail::state.preprocessing_finished.store(false, std::memory_order_relaxed);
    detail::state.preprocessing_component_split.store(false, std::memory_order_relaxed);
    detail::state.preprocessing_components_seen.store(0, std::memory_order_relaxed);
    detail::state.preprocessing_largest_component.store(0, std::memory_order_relaxed);
    detail::state.preprocessing_pending_components.store(0, std::memory_order_relaxed);
    detail::state.preprocessing_largest_pending_component.store(0, std::memory_order_relaxed);
    detail::state.preprocessing_reduction_child_checks.store(0, std::memory_order_relaxed);
    detail::state.preprocessing_maximum_reduction_depth.store(0, std::memory_order_relaxed);
    detail::state.preprocessing_reduction_decisions.store(0, std::memory_order_relaxed);
    detail::state.preprocessing_model_delegations.store(0, std::memory_order_relaxed);
}

inline void preprocessing_top_component(size_t component_dimension, bool split) noexcept
{
    if (!enabled()) return;
    detail::state.preprocessing_components_seen.fetch_add(1, std::memory_order_relaxed);
    const size_t largest = detail::state.preprocessing_largest_component.load(std::memory_order_relaxed);
    if (component_dimension > largest) {
        detail::state.preprocessing_largest_component.store(component_dimension, std::memory_order_relaxed);
    }
    if (split) detail::state.preprocessing_component_split.store(true, std::memory_order_relaxed);
}

inline void preprocessing_complete(size_t pending_components, size_t largest_pending_component) noexcept
{
    if (!enabled()) return;
    detail::state.preprocessing_pending_components.store(pending_components, std::memory_order_relaxed);
    detail::state.preprocessing_largest_pending_component.store(largest_pending_component, std::memory_order_relaxed);
    detail::state.preprocessing_finished.store(true, std::memory_order_relaxed);
}

inline void preprocessing_reduction_child(size_t depth) noexcept
{
    if (!enabled()) return;
    detail::state.preprocessing_reduction_child_checks.fetch_add(1, std::memory_order_relaxed);
    const size_t maximum = detail::state.preprocessing_maximum_reduction_depth.load(std::memory_order_relaxed);
    if (depth > maximum) detail::state.preprocessing_maximum_reduction_depth.store(depth, std::memory_order_relaxed);
}

inline void preprocessing_reduction_decision() noexcept
{
    if (enabled()) detail::state.preprocessing_reduction_decisions.fetch_add(1, std::memory_order_relaxed);
}

inline void preprocessing_model_delegation() noexcept
{
    if (enabled()) detail::state.preprocessing_model_delegations.fetch_add(1, std::memory_order_relaxed);
}

class tracker {
public:
    tracker(metric kind, size_t maximum) noexcept : active_(enabled()), kind_(kind), maximum_(maximum), current_(maximum)
    {
        if (!active_) return;
        detail::reset_current_metric();
        detail::state.maximum.store(maximum_, std::memory_order_relaxed);
        detail::state.current.store(current_, std::memory_order_relaxed);
        detail::state.kind.store(kind_, std::memory_order_relaxed);
    }

    bool active() const noexcept
    {
        return active_;
    }

    void stage(size_t current, size_t depth = 0, size_t open = 0) noexcept
    {
        if (!active_) return;
        current_ = current;
        depth_ = depth;
        open_ = open;
        publish();
    }

    void support_cardinality(size_t cardinality) noexcept
    {
        if (!active_) return;
        current_ = cardinality;
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
        detail::state.nodes.store(nodes_, std::memory_order_relaxed);
    }

    void visit_bracelet() noexcept
    {
        if (!active_) return;
        detail::increment(nodes_);
        if (nodes_ == 1 || (nodes_ & bracelet_publish_mask) == 0) publish();
    }

    void covered_support() noexcept
    {
        if (!active_) return;
        detail::increment(resolved_);
        if (kind_ == metric::support) detail::state.resolved.store(resolved_, std::memory_order_relaxed);
    }

    void skip_supports(uint64_t count) noexcept
    {
        if (!active_) return;
        nodes_ = count > std::numeric_limits<uint64_t>::max() - nodes_ ? std::numeric_limits<uint64_t>::max() : nodes_ + count;
        resolved_ = count > std::numeric_limits<uint64_t>::max() - resolved_ ? std::numeric_limits<uint64_t>::max() : resolved_ + count;
        detail::state.nodes.store(nodes_, std::memory_order_relaxed);
        detail::state.resolved.store(resolved_, std::memory_order_relaxed);
    }

    void resolved(long double weight = 0.0L) noexcept
    {
        if (!active_) return;
        detail::increment(resolved_);
        if (weight != 0.0L) coverage_ = std::min(1.0L, coverage_ + weight);
    }

    void secondary() noexcept
    {
        if (!active_) return;
        detail::increment(secondary_);
        if (kind_ == metric::support) detail::state.secondary.store(secondary_, std::memory_order_relaxed);
    }

    void support_lift_system(size_t dimension, size_t depth, size_t cache_size) noexcept
    {
        if (!active_) return;
        detail::increment(secondary_);
        detail::increment(lifted_processed_);
        lift_dimension_ = dimension;
        lift_depth_ = depth;
        lift_maximum_dimension_ = std::max(lift_maximum_dimension_, dimension);
        lift_maximum_depth_ = std::max(lift_maximum_depth_, depth);
        lift_cache_size_ = cache_size;
        publish_support_lift_event();
    }

    void support_lift_duplicate(size_t dimension, size_t depth, size_t cache_size) noexcept
    {
        if (!active_) return;
        detail::increment(lift_duplicate_skips_);
        lift_dimension_ = dimension;
        lift_depth_ = depth;
        lift_maximum_dimension_ = std::max(lift_maximum_dimension_, dimension);
        lift_maximum_depth_ = std::max(lift_maximum_depth_, depth);
        lift_cache_size_ = cache_size;
        publish_support_lift_event();
    }

    void support_lift_covered(size_t dimension, size_t depth, size_t cache_size) noexcept
    {
        if (!active_) return;
        detail::increment(lift_covered_skips_);
        lift_dimension_ = dimension;
        lift_depth_ = depth;
        lift_maximum_dimension_ = std::max(lift_maximum_dimension_, dimension);
        lift_maximum_depth_ = std::max(lift_maximum_depth_, depth);
        lift_cache_size_ = cache_size;
        publish_support_lift_event();
    }

    void support_lift_idle(size_t cache_size) noexcept
    {
        if (!active_) return;
        lift_dimension_ = 0;
        lift_depth_ = 0;
        lift_cache_size_ = cache_size;
        lift_frontier_size_ = 0;
        publish();
    }

    void support_lift_frontier(size_t frontier_size) noexcept
    {
        if (!active_) return;
        lift_frontier_size_ = frontier_size;
        lift_maximum_frontier_size_ = std::max(lift_maximum_frontier_size_, frontier_size);
    }

    void split() noexcept
    {
        if (active_) detail::increment(splits_);
    }

    void certificate() noexcept
    {
        if (!active_) return;
        detail::increment(splits_);
        if (kind_ == metric::support) detail::state.splits.store(splits_, std::memory_order_relaxed);
    }

    void certificate(size_t free_indices) noexcept
    {
        certificate();
        if (!active_ || free_indices > maximum_) return;
        std::lock_guard<std::mutex> lock(detail::state.certificate_histogram_mutex);
        ++detail::state.certificate_cardinality_free_index_counts[{current_, free_indices}];
    }

    void certificate(size_t free_indices, size_t upper_size) noexcept
    {
        certificate();
        if (!active_ || free_indices > upper_size || upper_size > maximum_) return;
        std::lock_guard<std::mutex> lock(detail::state.certificate_histogram_mutex);
        ++detail::state.certificate_cardinality_free_index_upper_size_counts[{current_, free_indices, upper_size}];
    }

    void singular_support(size_t nullity) noexcept
    {
        if (!active_ || nullity == 0) return;
        std::lock_guard<std::mutex> lock(detail::state.certificate_histogram_mutex);
        ++detail::state.singular_cardinality_nullity_counts[{current_, nullity}];
    }

    void lifted_certificate(size_t lifted_cardinality, size_t upper_size, size_t lower_size) noexcept
    {
        certificate();
        if (!active_ || current_ > lifted_cardinality || lifted_cardinality > upper_size || upper_size > maximum_ ||
            lower_size > lifted_cardinality)
            return;
        std::lock_guard<std::mutex> lock(detail::state.certificate_histogram_mutex);
        ++detail::state.certificate_root_lifted_upper_lower_counts[{current_, lifted_cardinality, upper_size, lower_size}];
    }

    void decision_diagram_cardinality(size_t cardinality, decision_diagram_phase phase) noexcept
    {
        if (!active_) return;
        current_ = cardinality;
        diagram_phase_ = phase;
        cardinality_started_ns_ =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        publish();
    }

    void decision_diagram_phase_change(decision_diagram_phase phase) noexcept
    {
        if (!active_) return;
        diagram_phase_ = phase;
        detail::state.diagram_phase.store(phase, std::memory_order_relaxed);
    }

    void decision_diagram_support() noexcept
    {
        if (!active_) return;
        detail::increment(nodes_);
        detail::state.nodes.store(nodes_, std::memory_order_relaxed);
    }

    void decision_diagram_certificate(size_t free_indices = std::numeric_limits<size_t>::max()) noexcept
    {
        if (!active_) return;
        detail::increment(resolved_);
        detail::state.resolved.store(resolved_, std::memory_order_relaxed);
        if (free_indices <= maximum_) {
            std::lock_guard<std::mutex> lock(detail::state.certificate_histogram_mutex);
            ++detail::state.certificate_cardinality_free_index_counts[{current_, free_indices}];
        }
    }

    void decision_diagram_certificate(size_t free_indices, size_t upper_size) noexcept
    {
        if (!active_) return;
        detail::increment(resolved_);
        detail::state.resolved.store(resolved_, std::memory_order_relaxed);
        if (free_indices <= upper_size && upper_size <= maximum_) {
            std::lock_guard<std::mutex> lock(detail::state.certificate_histogram_mutex);
            ++detail::state.certificate_cardinality_free_index_upper_size_counts[{current_, free_indices, upper_size}];
        }
    }

    void decision_diagram_certificate(size_t cardinality, size_t free_indices, size_t upper_size) noexcept
    {
        if (!active_) return;
        detail::increment(resolved_);
        detail::state.resolved.store(resolved_, std::memory_order_relaxed);
        if (cardinality > upper_size || free_indices > upper_size || upper_size > maximum_) return;
        std::lock_guard<std::mutex> lock(detail::state.certificate_histogram_mutex);
        ++detail::state.certificate_cardinality_free_index_upper_size_counts[{cardinality, free_indices, upper_size}];
    }

    void decision_diagram_work(size_t allocated_nodes, uint64_t operations) noexcept
    {
        if (!active_) return;
        secondary_ = allocated_nodes;
        splits_ = operations;
        detail::state.secondary.store(secondary_, std::memory_order_relaxed);
        detail::state.splits.store(splits_, std::memory_order_relaxed);
    }

    void finish() noexcept
    {
        if (active_) publish();
    }

    void adaptive_stage(adaptive_engine engine, adaptive_phase phase, size_t work_current = 0, size_t work_maximum = 0) noexcept
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
    void publish_support_lift_event() noexcept
    {
        detail::increment(support_lift_events_);
        if (support_lift_events_ == 1 || (support_lift_events_ & publish_mask) == 0) publish();
    }

    void publish() const noexcept
    {
        detail::state.nodes.store(nodes_, std::memory_order_relaxed);
        detail::state.resolved.store(resolved_, std::memory_order_relaxed);
        detail::state.secondary.store(secondary_, std::memory_order_relaxed);
        detail::state.splits.store(splits_, std::memory_order_relaxed);
        detail::state.open.store(open_, std::memory_order_relaxed);
        detail::state.coverage.store(static_cast<uint64_t>(std::clamp(coverage_, 0.0L, 1.0L) * coverage_scale), std::memory_order_relaxed);
        detail::state.current.store(current_, std::memory_order_relaxed);
        detail::state.depth.store(depth_, std::memory_order_relaxed);
        detail::state.lifted_processed.store(lifted_processed_, std::memory_order_relaxed);
        detail::state.lift_duplicate_skips.store(lift_duplicate_skips_, std::memory_order_relaxed);
        detail::state.lift_covered_skips.store(lift_covered_skips_, std::memory_order_relaxed);
        detail::state.lift_dimension.store(lift_dimension_, std::memory_order_relaxed);
        detail::state.lift_depth.store(lift_depth_, std::memory_order_relaxed);
        detail::state.lift_maximum_dimension.store(lift_maximum_dimension_, std::memory_order_relaxed);
        detail::state.lift_maximum_depth.store(lift_maximum_depth_, std::memory_order_relaxed);
        detail::state.lift_cache_size.store(lift_cache_size_, std::memory_order_relaxed);
        detail::state.lift_frontier_size.store(lift_frontier_size_, std::memory_order_relaxed);
        detail::state.lift_maximum_frontier_size.store(lift_maximum_frontier_size_, std::memory_order_relaxed);
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
        detail::state.diagram_phase.store(diagram_phase_, std::memory_order_relaxed);
        detail::state.cardinality_started_ns.store(cardinality_started_ns_, std::memory_order_relaxed);
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
    uint64_t lifted_processed_ = 0;
    uint64_t lift_duplicate_skips_ = 0;
    uint64_t lift_covered_skips_ = 0;
    uint64_t support_lift_events_ = 0;
    size_t lift_dimension_ = 0;
    size_t lift_depth_ = 0;
    size_t lift_maximum_dimension_ = 0;
    size_t lift_maximum_depth_ = 0;
    size_t lift_cache_size_ = 0;
    size_t lift_frontier_size_ = 0;
    size_t lift_maximum_frontier_size_ = 0;
    uint64_t open_ = 0;
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
    decision_diagram_phase diagram_phase_ = decision_diagram_phase::none;
    int64_t cardinality_started_ns_ = 0;
};

class reporter {
public:
    reporter(bool show_output, std::ostream& output, bool collect = false, bool capture_output = false)
        : active_(show_output || collect || capture_output), show_output_(show_output), capture_output_(capture_output),
          write_output_(show_output || capture_output), output_(output)
    {
        if (!active_) return;
        detail::reset();
        detail::state.enabled.store(true, std::memory_order_relaxed);
        started_ = std::chrono::steady_clock::now();
        if (write_output_) thread_ = std::thread([this] { run(); });
    }

    ~reporter()
    {
        stop();
    }

    reporter(const reporter&) = delete;
    reporter& operator=(const reporter&) = delete;

    void stop() noexcept
    {
        if (!active_) return;
        detail::state.enabled.store(false, std::memory_order_relaxed);
        if (write_output_) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stopped_ = true;
            }
            condition_.notify_one();
            if (thread_.joinable()) thread_.join();
            try {
                const auto now = std::chrono::steady_clock::now();
                const snapshot current = detail::load();
                if (current.kind != metric::none) {
                    const double seconds = std::chrono::duration<double>(now - last_report_).count();
                    const double rate =
                        current.nodes >= last_nodes_ && seconds > 0.0 ? static_cast<double>(current.nodes - last_nodes_) / seconds : 0.0;
                    write(detail::format(current, std::chrono::duration_cast<std::chrono::seconds>(now - started_), rate,
                                         current_elapsed(current, now)));
                }
            } catch (...) {
                // Diagnostics must never turn a completed exact decision into a
                // failure.
            }
        }
        active_ = false;
    }

private:
    void write(const std::string& line)
    {
        if (show_output_) output_ << line << '\n' << std::flush;
        if (capture_output_) {
            std::lock_guard<std::mutex> lock(detail::state.diagnostics_mutex);
            detail::state.diagnostics.append(line).push_back('\n');
        }
    }

    static std::chrono::seconds current_elapsed(const snapshot& current, std::chrono::steady_clock::time_point now) noexcept
    {
        if (current.cardinality_started_ns <= 0) return std::chrono::seconds(0);
        const auto started = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(current.cardinality_started_ns));
        if (started > now) return std::chrono::seconds(0);
        return std::chrono::duration_cast<std::chrono::seconds>(now - started);
    }

    void run()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!condition_.wait_for(lock, report_interval, [this] { return stopped_; })) {
            lock.unlock();
            const auto now = std::chrono::steady_clock::now();
            const snapshot current = detail::load();
            const double seconds = std::chrono::duration<double>(now - last_report_).count();
            const double rate =
                current.nodes >= last_nodes_ && seconds > 0.0 ? static_cast<double>(current.nodes - last_nodes_) / seconds : 0.0;
            write(detail::format(current, std::chrono::duration_cast<std::chrono::seconds>(now - started_), rate,
                                 current_elapsed(current, now)));
            last_report_ = now;
            last_nodes_ = current.nodes;
            lock.lock();
        }
    }

    bool active_;
    const bool show_output_;
    const bool capture_output_;
    const bool write_output_;
    std::ostream& output_;
    std::chrono::steady_clock::time_point started_{};
    std::chrono::steady_clock::time_point last_report_ = std::chrono::steady_clock::now();
    uint64_t last_nodes_ = 0;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopped_ = false;
    std::thread thread_;
};

} // namespace coposit::diagnostics
