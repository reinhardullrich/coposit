#pragma once

#ifdef COPOSIT_DENSE_BITSET_DICKINSON_TESTING

#include <cstddef>
#include <string_view>
#include <vector>

namespace coposit::dense_bitset_dickinson_diagnostics {

struct event {
    std::string_view name;
    size_t cardinality = 0;

    bool operator==(const event& other) const noexcept
    {
        return name == other.name && cardinality == other.cardinality;
    }
};

inline thread_local std::vector<event> events;

inline void clear()
{
    events.clear();
}

inline void record(std::string_view name, size_t cardinality)
{
    events.push_back({name, cardinality});
}

} // namespace coposit::dense_bitset_dickinson_diagnostics

#define COPOSIT_DENSE_BITSET_DIAGNOSTICS(...) ::coposit::dense_bitset_dickinson_diagnostics::record(__VA_ARGS__)

#else

#define COPOSIT_DENSE_BITSET_DIAGNOSTICS(...) ((void)0)

#endif
