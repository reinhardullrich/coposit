#pragma once

#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cstddef>
#include <vector>

namespace coposit::connected_components {

/*
 * Visit each component of an already scanned exact negative-entry graph using one reused support. The visitor receives the
 * component and whether it is the whole graph, and returns whether traversal should continue. The component reference is valid
 * only during that call. The return value is the number of visited components.
 */
template<typename Visitor>
inline size_t visit(const std::vector<support>& negative_neighbors, Visitor&& visitor)
{
    const size_t dimension = negative_neighbors.size();
    support remaining(dimension);
    for (size_t index = 0; index < dimension; ++index) remaining.set(index);
    support component(dimension);
    support frontier(dimension);
    support discovered(dimension);
    size_t visited_components = 0;

    while (!remaining.empty()) {
        timeout_checkpoint();
        component.clear();
        frontier.clear();
        const size_t first_vertex = remaining.lowest_index();
        component.set(first_vertex);
        frontier.set(first_vertex);

        while (!frontier.empty()) {
            timeout_checkpoint();
            const size_t vertex = frontier.lowest_index();
            frontier.reset(vertex);
            discovered = negative_neighbors[vertex];
            discovered.remove(component);
            component.add(discovered);
            frontier.add(discovered);
        }

        remaining.remove(component);
        ++visited_components;
        const support& current = component;
        if (!visitor(current, visited_components == 1 && remaining.empty())) break;
    }

    return visited_components;
}

} // namespace coposit::connected_components
