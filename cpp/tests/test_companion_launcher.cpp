#include "../companion_launcher.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    if (argc != 2) return 2;
    const auto started = std::chrono::steady_clock::now();
    const int status = coposit::cli::launch_companion(
        argv[1], {argv[1], "-E", "sleep", "2"}, coposit::cli::timeout_duration(0.05), "timeout test");
    const auto elapsed = std::chrono::steady_clock::now() - started;
    if (status != 124) {
        std::cerr << "expected timeout exit 124, got " << status << '\n';
        return 1;
    }
    if (elapsed >= std::chrono::seconds(1)) {
        std::cerr << "timed child was not stopped promptly\n";
        return 1;
    }
    return 0;
}
