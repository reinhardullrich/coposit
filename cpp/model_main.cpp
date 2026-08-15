#include <coposit/model.hpp>
#include <coposit/parsers/matrix_parser.hpp>
#include <coposit/progress.hpp>

#if defined(COPOSIT_PUBLIC_FAST) || defined(COPOSIT_PUBLIC_SAFE) || defined(COPOSIT_ANALYSIS_COMPANION)
#include <coposit/component_pipeline.hpp>
#endif

#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

std::string read_all(std::istream& input)
{
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

#ifndef COPOSIT_ANALYSIS_COMPANION
coposit::model::copositivity_mode parse_mode(const std::string& name)
{
    if (name == "copositive") return coposit::model::copositivity_mode::copositive;
    if (name == "strictly_copositive") return coposit::model::copositivity_mode::strictly_copositive;
    throw std::invalid_argument("mode must be 'copositive' or 'strictly_copositive'");
}
#endif

#ifdef COPOSIT_ANALYSIS_COMPANION
bool parse_switch(const std::string& name, const char* option)
{
    if (name == "on") return true;
    if (name == "off") return false;
    throw std::invalid_argument(std::string(option) + " must be 'on' or 'off'");
}
#endif

bool is_compact_matrix_argument(const std::string& argument) noexcept
{
    size_t index = 0;
    while (index < argument.size() && argument[index] >= '0' && argument[index] <= '9') ++index;
    return index > 0 && index < argument.size() && argument[index] == '#';
}

void print_usage()
{
#ifdef COPOSIT_PUBLIC_FAST
    std::cout << "Internal companion for the public coposit interface.\n"
                 "Method: fast (Adaptive Sponsel-COPOMATRIX)\n";
#elif defined(COPOSIT_PUBLIC_SAFE)
    std::cout << "Internal companion for the public coposit interface.\n"
                 "Method: safe (Dickinson Final)\n";
#elif defined(COPOSIT_ANALYSIS_COMPANION)
    std::cout << "Internal one-model companion for coposit-analyze. Run coposit-analyze --help for the public analysis interface.\n";
#endif
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage();
        return 0;
    }

    try {
        coposit::model::copositivity_mode mode = coposit::model::copositivity_mode::strictly_copositive;
        bool classify_both = false;
#ifdef COPOSIT_ANALYSIS_COMPANION
        bool mode_given = false;
#endif
        bool show_progress = false;
        std::string input_argument;
#if defined(COPOSIT_PUBLIC_FAST) || defined(COPOSIT_PUBLIC_SAFE) || defined(COPOSIT_ANALYSIS_COMPANION)
        coposit::component_pipeline::options preprocessing;
#endif
        for (int next_argument = 1; next_argument < argc; ++next_argument) {
            const std::string argument = argv[next_argument];
            if (argument == "--mode") {
                if (++next_argument == argc) throw std::invalid_argument("--mode requires a value");
#ifdef COPOSIT_PUBLIC_SAFE
                if (std::string(argv[next_argument]) == "both") classify_both = true;
                else mode = parse_mode(argv[next_argument]);
#elif defined(COPOSIT_ANALYSIS_COMPANION)
                if (mode_given) throw std::invalid_argument("--mode may be given only once");
                mode_given = true;
                if (std::string(argv[next_argument]) == "both") classify_both = true;
                else if (std::string(argv[next_argument]) == "strict") mode = coposit::model::copositivity_mode::strictly_copositive;
                else if (std::string(argv[next_argument]) == "non-strict") mode = coposit::model::copositivity_mode::copositive;
                else throw std::invalid_argument("mode must be 'strict', 'non-strict', or 'both'");
#else
                mode = parse_mode(argv[next_argument]);
#endif
#ifdef COPOSIT_ANALYSIS_COMPANION
            } else if (argument == "--preprocessing") {
                if (++next_argument == argc) throw std::invalid_argument("--preprocessing requires a value");
                preprocessing.preprocessing_enabled = parse_switch(argv[next_argument], "--preprocessing");
#endif
            } else if (argument == "--progress") {
                if (show_progress) throw std::invalid_argument("--progress may be given only once");
                show_progress = true;
            } else {
                if (!input_argument.empty()) throw std::invalid_argument("too many arguments");
                input_argument = argument;
            }
        }

#ifdef COPOSIT_ANALYSIS_COMPANION
        if (!mode_given) throw std::invalid_argument("--mode is required");
#ifndef COPOSIT_SUPPORTS_COPOSITIVE
        if (mode == coposit::model::copositivity_mode::copositive && !classify_both) {
            throw std::invalid_argument("this model supports only strict copositivity");
        }
#endif
#ifndef COPOSIT_HAS_COMBINED_CLASSIFICATION
        if (classify_both) throw std::invalid_argument("this model does not support combined classification");
#endif
#endif

        coposit::parsers::parsed_matrix parsed;
        if (!input_argument.empty() && input_argument != "-") {
            parsed = is_compact_matrix_argument(input_argument) ? coposit::parsers::matrix_parser::parse(input_argument)
                                                                : coposit::parsers::matrix_parser::parse_file(input_argument);
        } else {
            parsed = coposit::parsers::matrix_parser::parse(read_all(std::cin));
        }
        const coposit::matrix_integer& matrix = parsed.matrix;
        coposit::progress::reporter reporter(show_progress, std::cerr);
#if defined(COPOSIT_PUBLIC_SAFE) || defined(COPOSIT_HAS_COMBINED_CLASSIFICATION)
        if (classify_both) {
            const coposit::model::copositivity_classification result = coposit::component_pipeline::classify(
                matrix, preprocessing, [](const coposit::matrix_integer& part) { return coposit::model::classify(part); });
            reporter.stop();
            std::cout << "copositive=" << (result.is_copositive ? "true\n" : "false\n")
                      << "strictly_copositive=" << (result.is_strictly_copositive ? "true\n" : "false\n");
            return 0;
        }
#endif
#if defined(COPOSIT_PUBLIC_FAST) || defined(COPOSIT_PUBLIC_SAFE) || defined(COPOSIT_ANALYSIS_COMPANION)
        const bool result = coposit::component_pipeline::check(
            matrix, mode, preprocessing, [&](const coposit::matrix_integer& part) { return coposit::model::solve(part, mode); });
#else
        const bool result = coposit::model::solve(matrix, mode);
#endif
        reporter.stop();
        std::cout << (result ? "true\n" : "false\n");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << argv[0] << ": " << error.what() << '\n';
        return 2;
    }
}
