#include <coposit/component_pipeline.hpp>
#include <coposit/diagnostics.hpp>
#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>
#include <coposit/parsers/matrix_parser.hpp>
#include <coposit/timeout.hpp>

#include <chrono>
#include <csignal>
#include <iostream>
#include <iterator>
#include <optional>
#include <signal.h>
#include <stdexcept>
#include <string>

namespace {

constexpr int status_ok = 0;
constexpr int status_parse_error = 1;
constexpr int status_exec_error = 4;
constexpr int status_timeout = 5;
constexpr int status_node_limit = 6;

struct result {
    int status = status_exec_error;
    std::optional<bool> is_copositive;
    std::optional<bool> is_strictly_copositive;
    long long elapsed_ns = 0;
    std::string error_message;
};

std::string read_all(std::istream& input)
{
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool parse_switch(const std::string& name, const char* option)
{
    if (name == "on") return true;
    if (name == "off") return false;
    throw std::invalid_argument(std::string(option) + " must be 'on' or 'off'");
}

bool is_compact_matrix_argument(const std::string& argument) noexcept
{
    size_t index = 0;
    while (index < argument.size() && argument[index] >= '0' && argument[index] <= '9') ++index;
    return index > 0 && index < argument.size() && argument[index] == '#';
}

std::string hex_encode(const std::string& text)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(text.size() * 2);
    for (const unsigned char byte : text) {
        encoded.push_back(digits[byte >> 4]);
        encoded.push_back(digits[byte & 15]);
    }
    return encoded;
}

std::string certificate_distribution(const coposit::diagnostics::snapshot& snapshot)
{
    std::string output = "[";
    bool first = true;
    const auto append_separator = [&] {
        if (!first) output.push_back(',');
        first = false;
    };
    if (!snapshot.certificate_root_lifted_upper_lower_counts.empty()) {
        for (const auto& [key, count] : snapshot.certificate_root_lifted_upper_lower_counts) {
            append_separator();
            const auto& [root_cardinality, lifted_cardinality, upper_size, lower_size] = key;
            output += "[" + std::to_string(root_cardinality) + "," + std::to_string(lifted_cardinality) + "," +
                      std::to_string(upper_size) + "," + std::to_string(lower_size) + "," + std::to_string(count) + "]";
        }
    } else if (!snapshot.certificate_cardinality_free_index_upper_size_counts.empty()) {
        for (const auto& [key, count] : snapshot.certificate_cardinality_free_index_upper_size_counts) {
            append_separator();
            const auto& [cardinality, free_indices, upper_size] = key;
            output += "[" + std::to_string(cardinality) + "," + std::to_string(free_indices) + "," +
                      std::to_string(upper_size) + "," + std::to_string(count) + "]";
        }
    } else {
        for (const auto& [key, count] : snapshot.certificate_cardinality_free_index_counts) {
            append_separator();
            output += "[" + std::to_string(key.first) + "," + std::to_string(key.second) + "," + std::to_string(count) + "]";
        }
    }
    output.push_back(']');
    return output;
}

void timeout_signal_handler(int) noexcept
{
    coposit::request_timeout();
}

void install_timeout_handler()
{
#ifdef SIGUSR1
    struct sigaction action {};
    action.sa_handler = timeout_signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &action, nullptr) != 0) throw std::runtime_error("could not install timeout signal handler");
#endif
}

void print_machine_result(const result& value, const coposit::diagnostics::snapshot& snapshot, const std::string& diagnostics)
{
    const auto optional_bool = [](const std::optional<bool>& item) { return item ? (*item ? 1 : 0) : -1; };
    std::cout << "coposit_result=1\n"
              << "status=" << value.status << '\n'
              << "is_copositive=" << optional_bool(value.is_copositive) << '\n'
              << "is_strictly_copositive=" << optional_bool(value.is_strictly_copositive) << '\n'
              << "elapsed_ns=" << value.elapsed_ns << '\n'
              << "error_message_hex=" << hex_encode(value.error_message) << '\n'
              << "diagnostics_hex=" << hex_encode(diagnostics) << '\n'
              << "certificate_joint_distribution=" << certificate_distribution(snapshot) << '\n';
}

void print_usage()
{
    std::cout << "Internal one-model companion for coposit. Run coposit --help for the experiment interface.\n";
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage();
        return 0;
    }

    bool machine = false;
    try {
        install_timeout_handler();
        coposit::reset_timeout();
        coposit::model::copositivity_mode mode = coposit::model::copositivity_mode::strictly_copositive;
#ifdef COPOSIT_HAS_COMBINED_CLASSIFICATION
        bool classify_both = true;
#else
        bool classify_both = false;
#endif
        bool mode_given = false;
        bool show_diagnostics = false;
        bool collect_diagnostics = false;
        bool model_parameter_given = false;
        std::string model_parameter;
        std::string input_argument;
        coposit::component_pipeline::options preprocessing;
        for (int next_argument = 1; next_argument < argc; ++next_argument) {
            const std::string argument = argv[next_argument];
            if (argument == "--mode") {
                if (++next_argument == argc) throw std::invalid_argument("--mode requires a value");
                if (mode_given) throw std::invalid_argument("--mode may be given only once");
                mode_given = true;
                const std::string selected_mode = argv[next_argument];
                classify_both = selected_mode == "both";
                if (selected_mode == "strict") mode = coposit::model::copositivity_mode::strictly_copositive;
                else if (selected_mode == "non-strict") mode = coposit::model::copositivity_mode::copositive;
                else if (!classify_both)
                    throw std::invalid_argument("mode must be 'strict', 'non-strict', or 'both'");
            } else if (argument == "--preprocessing") {
                if (++next_argument == argc) throw std::invalid_argument("--preprocessing requires a value");
                preprocessing.preprocessing_enabled = parse_switch(argv[next_argument], "--preprocessing");
            } else if (argument == "--diagnostics") {
                if (show_diagnostics) throw std::invalid_argument("--diagnostics may be given only once");
                show_diagnostics = true;
            } else if (argument == "--model-parameter") {
                if (++next_argument == argc) throw std::invalid_argument("--model-parameter requires a value");
                if (model_parameter_given) throw std::invalid_argument("--model-parameter may be given only once");
                model_parameter_given = true;
                model_parameter = argv[next_argument];
            } else if (argument == "--machine") {
                if (machine) throw std::invalid_argument("--machine may be given only once");
                machine = true;
            } else if (argument == "--collect-diagnostics") {
                if (collect_diagnostics) throw std::invalid_argument("--collect-diagnostics may be given only once");
                collect_diagnostics = true;
            } else {
                if (!input_argument.empty()) throw std::invalid_argument("too many arguments");
                input_argument = argument;
            }
        }

#ifndef COPOSIT_HAS_COMBINED_CLASSIFICATION
        if (!mode_given) throw std::invalid_argument("--mode is required for a model without combined classification");
#endif
#ifndef COPOSIT_SUPPORTS_COPOSITIVE
        if (mode == coposit::model::copositivity_mode::copositive && !classify_both)
            throw std::invalid_argument("this model supports only strict copositivity");
#endif
#ifndef COPOSIT_HAS_COMBINED_CLASSIFICATION
        if (classify_both) throw std::invalid_argument("this model does not support combined classification");
#endif
#ifdef COPOSIT_HAS_MODEL_PARAMETER
        if (!model_parameter_given) throw std::invalid_argument("--model-parameter is required for this model");
        coposit::model::configure(model_parameter);
#else
        if (model_parameter_given) throw std::invalid_argument("this model does not accept --model-parameter");
#endif

        coposit::parsers::parsed_matrix parsed;
        try {
            if (!input_argument.empty() && input_argument != "-") {
                parsed = is_compact_matrix_argument(input_argument) ? coposit::parsers::matrix_parser::parse(input_argument)
                                                                    : coposit::parsers::matrix_parser::parse_file(input_argument);
            } else {
                parsed = coposit::parsers::matrix_parser::parse(read_all(std::cin));
            }
        } catch (const std::exception& error) {
            if (!machine) throw;
            print_machine_result({status_parse_error, std::nullopt, std::nullopt, 0, error.what()}, {}, {});
            return 0;
        }

        const coposit::matrix_integer& matrix = parsed.matrix;
        coposit::diagnostics::reporter reporter(show_diagnostics, std::cerr, collect_diagnostics, collect_diagnostics);
        const auto started = std::chrono::steady_clock::now();
        result solved;
        try {
#ifdef COPOSIT_HAS_COMBINED_CLASSIFICATION
            if (classify_both) {
                const coposit::model::copositivity_classification classification = coposit::component_pipeline::classify(
                    matrix, preprocessing, [](const coposit::matrix_integer& part) { return coposit::model::classify(part); });
                solved = {status_ok, classification.is_copositive, classification.is_strictly_copositive};
            } else
#endif
            {
                const bool classification = coposit::component_pipeline::check(
                    matrix, mode, preprocessing,
                    [&](const coposit::matrix_integer& part) { return coposit::model::solve(part, mode); });
                solved = mode == coposit::model::copositivity_mode::copositive
                    ? result{status_ok, classification, std::nullopt}
                    : result{status_ok, std::nullopt, classification};
            }
            coposit::timeout_checkpoint();
        } catch (const coposit::timeout_requested&) {
            solved = {status_timeout};
        } catch (const coposit::open_node_limit_reached& error) {
            solved = {status_node_limit, std::nullopt, std::nullopt, 0, error.what()};
        } catch (const std::exception& error) {
            solved = {status_exec_error, std::nullopt, std::nullopt, 0, error.what()};
        }
        solved.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count();
        coposit::reset_timeout();
        reporter.stop();

        if (machine) {
            const coposit::diagnostics::snapshot snapshot = collect_diagnostics ? coposit::diagnostics::detail::load()
                                                                                : coposit::diagnostics::snapshot{};
            const std::string diagnostic_text = collect_diagnostics ? coposit::diagnostics::detail::load_diagnostics() : std::string{};
            print_machine_result(solved, snapshot, diagnostic_text);
            return 0;
        }
        if (solved.status != status_ok) throw std::runtime_error(solved.error_message.empty() ? "model did not complete" : solved.error_message);
        if (classify_both) {
            std::cout << "copositive=" << (*solved.is_copositive ? "true\n" : "false\n")
                      << "strictly_copositive=" << (*solved.is_strictly_copositive ? "true\n" : "false\n");
        } else {
            const bool classification = solved.is_copositive ? *solved.is_copositive : *solved.is_strictly_copositive;
            std::cout << (classification ? "true\n" : "false\n");
        }
        return 0;
    } catch (const std::exception& error) {
        if (machine) {
            print_machine_result({status_exec_error, std::nullopt, std::nullopt, 0, error.what()}, {}, {});
            return 0;
        }
        std::cerr << argv[0] << ": " << error.what() << '\n';
        return 2;
    }
}
