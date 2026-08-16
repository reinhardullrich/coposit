#pragma once

#ifdef COPOSIT_BASELINE_SOURCE_DIAGNOSTICS

#include <cstddef>
#include <string_view>
#include <vector>

namespace coposit::baseline_source_diagnostics {

struct event {
    std::string_view name;
    size_t first = 0;
    size_t second = 0;

    bool operator==(const event& other) const noexcept
    {
        return name == other.name && first == other.first && second == other.second;
    }
};

inline thread_local std::vector<event> events;

inline void clear()
{
    events.clear();
}

inline void record(std::string_view name, size_t first = 0, size_t second = 0)
{
    events.push_back({name, first, second});
}

} // namespace coposit::baseline_source_diagnostics

#define COPOSIT_SOURCE_DIAGNOSTICS(...) ::coposit::baseline_source_diagnostics::record(__VA_ARGS__)

#else

#define COPOSIT_SOURCE_DIAGNOSTICS(...) ((void)0)

#endif
