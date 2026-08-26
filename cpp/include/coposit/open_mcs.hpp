/*
 * This file adapts the MCS maximum-clique algorithm from Open MCS:
 * https://github.com/darrenstrash/open-mcs, commit 735788af066fc8589f577036af521f22f45c2731.
 * Copyright (c) 2016 Darren Strash. Open MCS is licensed under GPL-3.0.
 *
 * coposit changes the container and integration code, uses size_t indices, checks its cooperative timeout, publishes diagnostics,
 * and permits a caller to stop once a proved clique reaches a decision threshold. The search order, greedy coloring bound, MCR
 * initial ordering, and MCS recoloring rule remain those of Open MCS.
 */

#pragma once

#include <coposit/diagnostics.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <list>
#include <utility>
#include <vector>

namespace coposit::open_mcs {

struct search_result {
    size_t best = 0;
    bool complete = false;
};

class maximum_clique_search {
public:
    maximum_clique_search(const support_context& support_context, const std::vector<support>& adjacency)
        : support_context_(support_context)
        , adjacency_(adjacency)
        , candidate_stack_(adjacency.size() + 1)
        , color_stack_(adjacency.size() + 1)
        , order_stack_(adjacency.size() + 1)
        , color_classes_(adjacency.size() + 1)
    {
    }

    maximum_clique_search(std::vector<support>&&) = delete;

    template<typename Stop>
    search_result run(Stop&& stop)
    {
        std::vector<size_t>& candidates = candidate_stack_[0];
        std::vector<size_t>& colors = color_stack_[0];
        std::vector<size_t>& order = order_stack_[0];
        initialize_order(candidates, colors);
        order = candidates;

        if (stop(best_)) return {best_, false};
        const bool stopped = candidates.empty() ? false : expand(candidates, order, colors, stop);
        diagnostics::advance_preprocessing(nodes_, 0);
        return {best_, !stopped};
    }

private:
    bool adjacent(size_t left, size_t right) const noexcept
    {
        return support_context_.contains(adjacency_[left], right);
    }

    void greedy_color(const std::vector<size_t>& evaluation_order, std::vector<size_t>& color_order, std::vector<size_t>& colors)
    {
        if (evaluation_order.empty()) return;
        size_t maximum_color = 0;
        for (const size_t vertex : evaluation_order) {
            timeout_checkpoint();
            size_t color = 0;
            while (!color_classes_[color].empty()) {
                bool conflict = false;
                for (const size_t colored_vertex : color_classes_[color]) {
                    if (adjacent(vertex, colored_vertex)) {
                        conflict = true;
                        break;
                    }
                }
                if (!conflict) break;
                ++color;
            }
            color_classes_[color].push_back(vertex);
            maximum_color = std::max(maximum_color, color);
        }

        size_t output = 0;
        for (size_t color = 0; color <= maximum_color; ++color) {
            for (const size_t vertex : color_classes_[color]) {
                color_order[output] = vertex;
                colors[output++] = color + 1;
            }
            color_classes_[color].clear();
        }
    }

    bool has_conflict(size_t vertex, const std::vector<size_t>& color_class) const
    {
        for (const size_t colored_vertex : color_class) {
            if (adjacent(vertex, colored_vertex)) return true;
        }
        return false;
    }

    size_t unique_conflict(size_t vertex, const std::vector<size_t>& color_class) const
    {
        size_t conflict = no_vertex;
        for (const size_t colored_vertex : color_class) {
            if (!adjacent(vertex, colored_vertex)) continue;
            if (conflict != no_vertex) return no_vertex;
            conflict = colored_vertex;
        }
        return conflict;
    }

    bool repair(size_t vertex, size_t old_color, size_t best_delta)
    {
        for (size_t new_color = 0; new_color < best_delta; ++new_color) {
            const size_t conflicting_vertex = unique_conflict(vertex, color_classes_[new_color]);
            if (conflicting_vertex == no_vertex) continue;
            for (size_t next_color = new_color + 1; next_color <= best_delta; ++next_color) {
                if (has_conflict(conflicting_vertex, color_classes_[next_color])) continue;
                auto& old_class = color_classes_[old_color];
                old_class.erase(std::find(old_class.begin(), old_class.end(), vertex));
                auto& new_class = color_classes_[new_color];
                new_class.erase(std::find(new_class.begin(), new_class.end(), conflicting_vertex));
                new_class.push_back(vertex);
                color_classes_[next_color].push_back(conflicting_vertex);
                return true;
            }
        }
        return false;
    }

    void recolor(const std::vector<size_t>& evaluation_order, std::vector<size_t>& color_order, std::vector<size_t>& colors)
    {
        if (evaluation_order.empty()) return;
        size_t maximum_color = 0;
        const bool can_repair = best_ >= clique_.size();
        const size_t best_delta = can_repair ? best_ - clique_.size() : 0;

        for (const size_t vertex : evaluation_order) {
            timeout_checkpoint();
            size_t color = 0;
            while (!color_classes_[color].empty()) {
                bool conflict = false;
                for (const size_t colored_vertex : color_classes_[color]) {
                    if (adjacent(vertex, colored_vertex)) {
                        conflict = true;
                        break;
                    }
                }
                if (!conflict) break;
                ++color;
            }
            color_classes_[color].push_back(vertex);
            maximum_color = std::max(maximum_color, color);
            if (can_repair && color + 1 > best_delta && color == maximum_color) {
                repair(vertex, color, best_delta);
                if (color_classes_[maximum_color].empty()) --maximum_color;
            }
        }

        size_t output = 0;
        for (size_t color = 0; color <= maximum_color; ++color) {
            for (const size_t vertex : color_classes_[color]) {
                color_order[output] = vertex;
                colors[output++] = color + 1;
            }
            color_classes_[color].clear();
        }
    }

    void initialize_order(std::vector<size_t>& ordered, std::vector<size_t>& colors)
    {
        const size_t dimension = adjacency_.size();
        ordered.resize(dimension);
        colors.resize(dimension);
        if (dimension == 0) return;

        std::vector<std::vector<size_t>> neighbors(dimension);
        std::vector<std::ptrdiff_t> degree(dimension);
        std::vector<std::list<size_t>> vertices_by_degree(dimension);
        std::vector<std::list<size_t>::iterator> location(dimension);
        size_t maximum_degree = 0;

        for (size_t vertex = 0; vertex < dimension; ++vertex) {
            support_context_.extract_set_indices(adjacency_[vertex], neighbors[vertex]);
            degree[vertex] = static_cast<std::ptrdiff_t>(neighbors[vertex].size());
            location[vertex] = vertices_by_degree[static_cast<size_t>(degree[vertex])].insert(
                vertices_by_degree[static_cast<size_t>(degree[vertex])].end(), vertex);
            maximum_degree = std::max(maximum_degree, neighbors[vertex].size());
        }

        size_t current_degree = 0;
        size_t removed = 0;
        while (removed < dimension) {
            timeout_checkpoint();
            if (vertices_by_degree[current_degree].empty()) {
                ++current_degree;
                continue;
            }

            const size_t remaining = dimension - removed;
            if (vertices_by_degree[current_degree].size() == remaining) {
                if (remaining == current_degree + 1) best_ = std::max(best_, remaining);
                std::vector<size_t> regular(vertices_by_degree[current_degree].begin(), vertices_by_degree[current_degree].end());
                std::vector<size_t> regular_colors(regular.size());
                greedy_color(regular, regular, regular_colors);

                size_t index = 0;
                size_t maximum_regular_color = 0;
                for (; index < regular.size(); ++index) {
                    ordered[index] = regular[index];
                    colors[index] = regular_colors[index];
                    maximum_regular_color = std::max(maximum_regular_color, colors[index]);
                }

                const size_t last_smaller_color =
                    std::min(regular.size() + maximum_degree - maximum_regular_color, dimension - 1);
                size_t color = maximum_regular_color + 1;
                while (index <= last_smaller_color) colors[index++] = color++;
                while (index < dimension) colors[index++] = maximum_degree + 1;
                return;
            }

            size_t vertex = vertices_by_degree[current_degree].front();
            if (vertices_by_degree[current_degree].size() > 1) {
                size_t minimum_neighborhood_degree = std::numeric_limits<size_t>::max();
                for (const size_t candidate : vertices_by_degree[current_degree]) {
                    size_t neighborhood_degree = 0;
                    for (const size_t neighbor : neighbors[candidate]) {
                        if (degree[neighbor] >= 0) neighborhood_degree += static_cast<size_t>(degree[neighbor]);
                    }
                    if (neighborhood_degree < minimum_neighborhood_degree) {
                        minimum_neighborhood_degree = neighborhood_degree;
                        vertex = candidate;
                    }
                }
                vertices_by_degree[current_degree].erase(location[vertex]);
            } else {
                vertices_by_degree[current_degree].pop_front();
            }

            ordered[dimension - removed - 1] = vertex;
            degree[vertex] = -1;
            for (const size_t neighbor : neighbors[vertex]) {
                if (degree[neighbor] < 0) continue;
                const size_t old_degree = static_cast<size_t>(degree[neighbor]);
                vertices_by_degree[old_degree].erase(location[neighbor]);
                --degree[neighbor];
                const size_t new_degree = static_cast<size_t>(degree[neighbor]);
                location[neighbor] = vertices_by_degree[new_degree].insert(vertices_by_degree[new_degree].end(), neighbor);
            }
            ++removed;
            current_degree = 0;
        }
    }

    template<typename Stop>
    bool expand(std::vector<size_t>& candidates, std::vector<size_t>& order, std::vector<size_t>& colors, Stop& stop)
    {
        timeout_checkpoint();
        ++nodes_;
        if ((nodes_ & diagnostics::publish_mask) == 0) diagnostics::advance_preprocessing(nodes_, 0);

        std::vector<size_t>& child_candidates = candidate_stack_[clique_.size() + 1];
        std::vector<size_t>& child_colors = color_stack_[clique_.size() + 1];
        std::vector<size_t>& child_order = order_stack_[clique_.size() + 1];

        while (!candidates.empty()) {
            if (clique_.size() + colors.back() <= best_) {
                candidates.clear();
                return false;
            }

            colors.pop_back();
            const size_t vertex = candidates.back();
            candidates.pop_back();

            child_order.clear();
            for (const size_t candidate : order) {
                if (adjacent(vertex, candidate)) child_order.push_back(candidate);
            }
            clique_.push_back(vertex);

            if (!child_order.empty()) {
                child_candidates.resize(child_order.size());
                child_colors.resize(child_order.size());
                recolor(child_order, child_candidates, child_colors);
                if (expand(child_candidates, child_order, child_colors, stop)) return true;
            } else if (clique_.size() > best_) {
                best_ = clique_.size();
                if (stop(best_)) return true;
            }

            order.erase(std::find(order.begin(), order.end(), vertex));
            clique_.pop_back();
        }
        return false;
    }

    static constexpr size_t no_vertex = std::numeric_limits<size_t>::max();
    const support_context& support_context_;
    const std::vector<support>& adjacency_;
    std::vector<size_t> clique_;
    std::vector<std::vector<size_t>> candidate_stack_;
    std::vector<std::vector<size_t>> color_stack_;
    std::vector<std::vector<size_t>> order_stack_;
    std::vector<std::vector<size_t>> color_classes_;
    size_t best_ = 0;
    size_t nodes_ = 0;
};

} // namespace coposit::open_mcs
