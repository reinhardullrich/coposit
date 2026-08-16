#pragma once

#ifdef COPOSIT_CLINGO_SAT_DICKINSON_TESTING

#include <cstddef>
#include <string_view>
#include <vector>

namespace coposit::clingo_sat_dickinson_diagnostics {

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

} // namespace coposit::clingo_sat_dickinson_diagnostics

#define COPOSIT_CLINGO_SAT_DIAGNOSTICS(...) ::coposit::clingo_sat_dickinson_diagnostics::record(__VA_ARGS__)

#else

#define COPOSIT_CLINGO_SAT_DIAGNOSTICS(...) ((void)0)

#endif
