#pragma once

#ifdef COPOSIT_CEILING_PRUNED_DICKINSON_TESTING

#include <cstddef>
#include <string_view>
#include <vector>

namespace coposit::ceiling_pruned_dickinson_trace {

struct event {
    std::string_view name;
    size_t cardinality = 0;

    bool operator==(const event& other) const noexcept
    {
        return name == other.name && cardinality == other.cardinality;
    }
};

inline thread_local std::vector<event> events;

inline void clear() { events.clear(); }
inline void record(std::string_view name, size_t cardinality) { events.push_back({name, cardinality}); }

} // namespace coposit::ceiling_pruned_dickinson_trace

#define COPOSIT_CEILING_DICKINSON_TRACE(...) ::coposit::ceiling_pruned_dickinson_trace::record(__VA_ARGS__)

#else

#define COPOSIT_CEILING_DICKINSON_TRACE(...) ((void)0)

#endif
