#pragma once

#include <cstddef>
#include <exception>

namespace coposit {

inline constexpr size_t maximum_open_nodes = 50000;

class open_node_limit_reached final : public std::exception {
public:
    const char* what() const noexcept override { return "maximum of 50000 open nodes reached"; }
};

inline void enforce_open_node_limit(size_t open_nodes)
{
    if (open_nodes > maximum_open_nodes) throw open_node_limit_reached{};
}

} // namespace coposit
