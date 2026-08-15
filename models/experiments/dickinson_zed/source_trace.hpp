#pragma once

#ifdef COPOSIT_DICKINSON_ZED_TESTING

#include <cstddef>
#include <string_view>
#include <vector>

namespace coposit::dickinson_zed_trace {

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

} // namespace coposit::dickinson_zed_trace

#define COPOSIT_DICKINSON_ZED_TRACE(...) ::coposit::dickinson_zed_trace::record(__VA_ARGS__)

#else

#define COPOSIT_DICKINSON_ZED_TRACE(...) ((void)0)

#endif
