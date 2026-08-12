#include "companion_launcher.hpp"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void print_usage(const char* program)
{
    std::cout << "Usage: " << program << " fast strict|non-strict [--progress] [--timeout SECONDS] [MATRIX|FILE|-]\n"
                 "       " << program << " safe strict|non-strict|both [--progress] [--timeout SECONDS] [MATRIX|FILE|-]\n"
                 "  fast  Adaptive Sponsel-COPOMATRIX; exact answers, but the bounded traversal may stop unresolved.\n"
                 "  safe  Dickinson Final; complete exact certificate enumeration when allowed to finish.\n"
                 "Both methods run connected-component splitting followed by all pre-checks.\n"
                 "MATRIX is compact dimension#values; otherwise the argument is a file path.\n"
                 "Input is read from standard input when the argument is omitted or '-'.\n"
                 "--progress writes a status line to standard error every second.\n"
                 "--timeout stops the complete command after the given positive number of seconds and exits 124.\n";
}

std::string companion_path(const char* launcher, const std::string& method)
{
    const std::string launcher_path = launcher;
    const size_t separator = launcher_path.find_last_of("/\\");
    const std::string directory = separator == std::string::npos ? std::string{} : launcher_path.substr(0, separator + 1);
#ifdef _WIN32
    return directory + "coposit-" + method + ".exe";
#else
    return directory + "coposit-" + method;
#endif
}

int launch(const std::string& program, const std::string& display_name, const std::string& internal_mode, int argc, char* argv[],
           const std::optional<coposit::cli::timeout_duration>& timeout)
{
    std::vector<std::string> arguments{display_name, "--mode", internal_mode};
    arguments.reserve(static_cast<size_t>(argc));
    for (int index = 3; index < argc; ++index) {
        if (std::string(argv[index]) == "--timeout") {
            ++index;
            continue;
        }
        arguments.emplace_back(argv[index]);
    }
    return coposit::cli::launch_companion(program, std::move(arguments), timeout, display_name);
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return 0;
    }

    try {
        if (argc < 2) throw std::invalid_argument("method must be 'fast' or 'safe'");
        const std::string method = argv[1];
        if (method != "fast" && method != "safe") throw std::invalid_argument("method must be 'fast' or 'safe'");
        if (argc < 3) throw std::invalid_argument("predicate must be 'strict', 'non-strict', or 'both'");
        bool progress = false;
        bool input = false;
        std::optional<coposit::cli::timeout_duration> timeout;
        for (int index = 3; index < argc; ++index) {
            if (std::string(argv[index]) == "--progress") {
                if (progress) throw std::invalid_argument("--progress may be given only once");
                progress = true;
            } else if (std::string(argv[index]) == "--timeout") {
                if (timeout) throw std::invalid_argument("--timeout may be given only once");
                if (++index == argc) throw std::invalid_argument("--timeout requires a value");
                timeout = coposit::cli::parse_timeout_seconds(argv[index]);
            } else {
                if (input) throw std::invalid_argument("too many arguments");
                input = true;
            }
        }
        const std::string predicate = argv[2];
        if (predicate != "strict" && predicate != "non-strict" && predicate != "both") {
            throw std::invalid_argument("predicate must be 'strict', 'non-strict', or 'both'");
        }
        if (predicate == "both" && method != "safe") throw std::invalid_argument("'both' is available only with 'safe'");
        const std::string internal_mode =
            predicate == "strict" ? "strictly_copositive" : predicate == "non-strict" ? "copositive" : "both";
        return launch(companion_path(argv[0], method), "coposit " + method + " " + predicate, internal_mode, argc, argv, timeout);
    } catch (const std::exception& error) {
        std::cerr << argv[0] << ": " << error.what() << '\n';
        return 2;
    }
}
