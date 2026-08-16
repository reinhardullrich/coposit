#pragma once

#ifdef COPOSIT_WIDE_CERTIFICATE_SAT_DICKINSON_TESTING

#include <cstddef>
#include <string_view>
#include <vector>

namespace coposit::wide_certificate_sat_dickinson_diagnostics {

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

} // namespace coposit::wide_certificate_sat_dickinson_diagnostics

#define COPOSIT_WIDE_CERTIFICATE_SAT_DIAGNOSTICS(...) \
    ::coposit::wide_certificate_sat_dickinson_diagnostics::record(__VA_ARGS__)

#else

#define COPOSIT_WIDE_CERTIFICATE_SAT_DIAGNOSTICS(...) ((void)0)

#endif
