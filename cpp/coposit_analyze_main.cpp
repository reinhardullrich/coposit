#include "companion_launcher.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* models[] = {
    "dutour_2018",
    "danninger_1990",
    "copomatrix_2011",
    "adaptive_sponsel_copomatrix",
    "hadeler_1983",
    "dickinson_final",
    "safi_2021",
    "bundfuss_2008",
    "sponsel_2012",
};

void print_usage(const char* program)
{
    std::cout << "Usage: " << program << " --model MODEL --mode strict|non-strict|both [OPTIONS] [MATRIX|FILE|-]\n"
                 "Options:\n"
                 "  --preprocessing on|off\n"
                 "  --progress\n"
                 "  --timeout SECONDS\n"
                 "The fixed preprocessing pipeline is on by default. Combined mode is available only for models that implement it.\n"
                 "Models:\n";
    for (const char* model : models) std::cout << "  " << model << '\n';
}

bool known_model(const std::string& name)
{
    return std::find(std::begin(models), std::end(models), name) != std::end(models);
}

std::string companion_path(const char* launcher, const std::string& model)
{
    const std::string launcher_path = launcher;
    const size_t separator = launcher_path.find_last_of("/\\");
    const std::string directory = separator == std::string::npos ? std::string{} : launcher_path.substr(0, separator + 1);
#ifdef _WIN32
    return directory + "coposit-analyze-" + model + ".exe";
#else
    return directory + "coposit-analyze-" + model;
#endif
}

int launch(const std::string& program, const std::string& display_name, int argc, char* argv[],
           const std::optional<coposit::cli::timeout_duration>& timeout)
{
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<size_t>(argc));
    arguments.push_back(display_name);
    for (int index = 1; index < argc; ++index) {
        if (std::string(argv[index]) == "--model") {
            ++index;
            continue;
        }
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
        std::string model;
        std::optional<coposit::cli::timeout_duration> timeout;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--model") {
                if (!model.empty()) throw std::invalid_argument("--model may be given only once");
                if (++index == argc) throw std::invalid_argument("--model requires a value");
                model = argv[index];
            } else if (argument == "--timeout") {
                if (timeout) throw std::invalid_argument("--timeout may be given only once");
                if (++index == argc) throw std::invalid_argument("--timeout requires a value");
                timeout = coposit::cli::parse_timeout_seconds(argv[index]);
            }
        }
        if (model.empty()) throw std::invalid_argument("--model is required");
        if (!known_model(model)) throw std::invalid_argument("unknown model: " + model);
        return launch(companion_path(argv[0], model), "coposit-analyze --model " + model, argc, argv, timeout);
    } catch (const std::exception& error) {
        std::cerr << argv[0] << ": " << error.what() << '\n';
        return 2;
    }
}
