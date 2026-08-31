#pragma once

#ifdef COPOSIT_IMPROVED_NBC_X6_TESTING

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace coposit::improved_nbc_x6_diagnostics {

struct event {
    std::string_view name;
    size_t cardinality = 0;

    bool operator==(const event& other) const noexcept
    {
        return name == other.name && cardinality == other.cardinality;
    }
};

struct interval_event {
    std::string_view stage;
    size_t detail = 0;
    std::vector<size_t> lower;
    std::vector<size_t> upper;
};

inline thread_local std::vector<event> events;
inline thread_local std::vector<interval_event> interval_events;

inline void clear()
{
    events.clear();
    interval_events.clear();
}
inline void record(std::string_view name, size_t cardinality) { events.push_back({name, cardinality}); }
inline void record_interval(std::string_view stage, size_t detail, std::vector<size_t> lower, std::vector<size_t> upper)
{
    interval_events.push_back({stage, detail, std::move(lower), std::move(upper)});
}

} // namespace coposit::improved_nbc_x6_diagnostics

#define COPOSIT_IMPROVED_NBC_X6_DIAGNOSTICS(...) ::coposit::improved_nbc_x6_diagnostics::record(__VA_ARGS__)

#else

#define COPOSIT_IMPROVED_NBC_X6_DIAGNOSTICS(...) ((void)0)

#endif
