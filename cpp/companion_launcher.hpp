#pragma once

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace coposit::cli {

using timeout_duration = std::chrono::duration<double>;

inline timeout_duration parse_timeout_seconds(const std::string& text)
{
    size_t parsed = 0;
    double seconds = 0.0;
    try {
        seconds = std::stod(text, &parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument("--timeout requires a positive number of seconds");
    }
    if (parsed != text.size() || !std::isfinite(seconds) || seconds <= 0.0) {
        throw std::invalid_argument("--timeout requires a positive number of seconds");
    }
    return timeout_duration(seconds);
}

inline int launch_companion(const std::string& program, std::vector<std::string> arguments,
                            const std::optional<timeout_duration>& timeout, const std::string& display_name)
{
#ifdef _WIN32
    std::vector<const char*> native_arguments;
    native_arguments.reserve(arguments.size() + 1);
    for (const std::string& argument : arguments) native_arguments.push_back(argument.c_str());
    native_arguments.push_back(nullptr);

    if (!timeout) {
        const intptr_t status = _spawnvp(_P_WAIT, program.c_str(), native_arguments.data());
        if (status == -1) throw std::system_error(errno, std::generic_category(), "cannot start " + program);
        return static_cast<int>(status);
    }

    const intptr_t raw_process = _spawnvp(_P_NOWAIT, program.c_str(), native_arguments.data());
    if (raw_process == -1) throw std::system_error(errno, std::generic_category(), "cannot start " + program);
    const HANDLE process = reinterpret_cast<HANDLE>(raw_process);
    const auto deadline = std::chrono::steady_clock::now() + *timeout;
    for (;;) {
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) {
            TerminateProcess(process, 124);
            WaitForSingleObject(process, INFINITE);
            CloseHandle(process);
            std::cerr << display_name << ": timed out after " << timeout->count() << " seconds\n";
            return 124;
        }
        const auto requested = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
        const DWORD wait_milliseconds = static_cast<DWORD>(std::min<long long>(requested, INFINITE - 1));
        const DWORD wait_result = WaitForSingleObject(process, wait_milliseconds);
        if (wait_result == WAIT_TIMEOUT) continue;
        if (wait_result != WAIT_OBJECT_0) {
            const DWORD error = GetLastError();
            CloseHandle(process);
            throw std::system_error(static_cast<int>(error), std::system_category(), "cannot wait for " + program);
        }
        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process, &exit_code)) {
            const DWORD error = GetLastError();
            CloseHandle(process);
            throw std::system_error(static_cast<int>(error), std::system_category(), "cannot read exit status for " + program);
        }
        CloseHandle(process);
        return static_cast<int>(exit_code);
    }
#else
    std::vector<char*> native_arguments;
    native_arguments.reserve(arguments.size() + 1);
    for (std::string& argument : arguments) native_arguments.push_back(argument.data());
    native_arguments.push_back(nullptr);

    if (!timeout) {
        execvp(program.c_str(), native_arguments.data());
        throw std::system_error(errno, std::generic_category(), "cannot start " + program);
    }

    const pid_t child = fork();
    if (child == -1) throw std::system_error(errno, std::generic_category(), "cannot start " + program);
    if (child == 0) {
        execvp(program.c_str(), native_arguments.data());
        const std::string message = program + ": " + std::strerror(errno) + "\n";
        static_cast<void>(write(STDERR_FILENO, message.data(), message.size()));
        _exit(126);
    }

    std::mutex mutex;
    std::condition_variable finished_condition;
    bool finished = false;
    std::atomic_bool expired{false};
    std::thread watchdog;
    try {
        watchdog = std::thread([&] {
            std::unique_lock<std::mutex> lock(mutex);
            if (finished_condition.wait_for(lock, *timeout, [&] { return finished; })) return;
            expired.store(true, std::memory_order_relaxed);
            static_cast<void>(kill(child, SIGKILL));
        });
    } catch (...) {
        static_cast<void>(kill(child, SIGKILL));
        static_cast<void>(waitpid(child, nullptr, 0));
        throw;
    }

    int status = 0;
    pid_t waited = -1;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited == -1 && errno == EINTR);
    const int wait_error = errno;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        finished = true;
    }
    finished_condition.notify_one();
    watchdog.join();

    if (waited == -1) throw std::system_error(wait_error, std::generic_category(), "cannot wait for " + program);
    if (expired.load(std::memory_order_relaxed)) {
        std::cerr << display_name << ": timed out after " << timeout->count() << " seconds\n";
        return 124;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 2;
#endif
}

} // namespace coposit::cli
