#pragma once

#ifdef COPOSIT_CLINGO_SAT_ZED_DICKINSON_TESTING

#include <cstddef>
#include <string_view>
#include <vector>

namespace coposit::clingo_sat_zed_dickinson_trace {

struct event {
    std::string_view name;
    size_t cardinality = 0;

    bool operator==(const event& other) const noexcept
    {
        return name == other.name && cardinality == other.cardinality;
    }
};

inline std::vector<event> events;

inline void clear() { events.clear(); }
inline void record(std::string_view name, size_t cardinality) { events.push_back({name, cardinality}); }

} // namespace coposit::clingo_sat_zed_dickinson_trace

#define COPOSIT_CLINGO_SAT_ZED_TRACE(...) ::coposit::clingo_sat_zed_dickinson_trace::record(__VA_ARGS__)

#else

#define COPOSIT_CLINGO_SAT_ZED_TRACE(...) ((void)0)

#endif
