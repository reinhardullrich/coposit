#pragma once

#include <csignal>

namespace coposit {

struct timeout_requested {};

#ifdef COPOSIT_ENABLE_TIMEOUTS

inline volatile std::sig_atomic_t timeout_signal_received = 0;

inline void request_timeout() noexcept
{
    timeout_signal_received = 1;
}

inline void reset_timeout() noexcept
{
    timeout_signal_received = 0;
}

inline bool timeout_pending() noexcept
{
    return timeout_signal_received != 0;
}

inline void timeout_checkpoint()
{
    if (timeout_pending()) throw timeout_requested{};
}

#else

inline void reset_timeout() noexcept {}
inline bool timeout_pending() noexcept { return false; }
inline void timeout_checkpoint() noexcept {}

#endif

} // namespace coposit
