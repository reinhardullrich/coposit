#pragma once

#include <coposit/support.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/timeout.hpp>

#include <cassert>
#include <cstddef>
#include <vector>

namespace coposit::connected_components {

/*
 * Visit each component of an already scanned exact negative-entry graph using one reused support. The visitor receives the
 * component and whether it is the whole graph, and returns whether traversal should continue. The component reference is valid
 * only during that call. The return value is the number of visited components.
 */
template<typename Visitor>
inline size_t visit(support_context& context, const std::vector<support>& negative_neighbors, Visitor&& visitor)
{
    const size_t dimension = negative_neighbors.size();
    assert(context.dimension() == dimension);
    support remaining = context.make();
    context.set_all(remaining);
    support component = context.make();
    support frontier = context.make();
    support discovered = context.make();
    size_t visited_components = 0;
    size_t visited_vertices = 0;

    while (!context.empty(remaining)) {
        timeout_checkpoint();
        context.clear(component);
        context.clear(frontier);
        const size_t first_vertex = context.first(remaining);
        context.set(component, first_vertex);
        context.set(frontier, first_vertex);

        while (!context.empty(frontier)) {
            timeout_checkpoint();
            const size_t vertex = context.first(frontier);
            context.reset(frontier, vertex);
            diagnostics::advance_preprocessing(++visited_vertices, dimension);
            context.copy(discovered, negative_neighbors[vertex]);
            context.subtract(discovered, component);
            context.add(component, discovered);
            context.add(frontier, discovered);
        }

        context.subtract(remaining, component);
        ++visited_components;
        const support& current = component;
        if (!visitor(current, visited_components == 1 && context.empty(remaining))) break;
    }

    context.release(std::move(remaining));
    context.release(std::move(component));
    context.release(std::move(frontier));
    context.release(std::move(discovered));
    return visited_components;
}

} // namespace coposit::connected_components
