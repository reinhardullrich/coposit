#pragma once

#ifdef COPOSIT_MULTITHREADED_CBDD_ZED_DICKINSON_TESTING

#include <cstddef>
#include <mutex>
#include <string_view>
#include <vector>

namespace coposit::multithreaded_cbdd_zed_dickinson_trace {

struct event {
    std::string_view name;
    size_t cardinality = 0;

    bool operator==(const event& other) const noexcept
    {
        return name == other.name && cardinality == other.cardinality;
    }
};

inline std::mutex mutex;
inline std::vector<event> events;

inline void clear()
{
    std::lock_guard<std::mutex> lock(mutex);
    events.clear();
}

inline void record(std::string_view name, size_t cardinality)
{
    std::lock_guard<std::mutex> lock(mutex);
    events.push_back({name, cardinality});
}

} // namespace coposit::multithreaded_cbdd_zed_dickinson_trace

#define COPOSIT_MULTITHREADED_CBDD_ZED_TRACE(...) ::coposit::multithreaded_cbdd_zed_dickinson_trace::record(__VA_ARGS__)

#else

#define COPOSIT_MULTITHREADED_CBDD_ZED_TRACE(...) ((void)0)

#endif
