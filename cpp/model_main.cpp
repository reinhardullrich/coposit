#include <coposit/model.hpp>
#include <coposit/parse_integer_matrix.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

std::string read_all(std::istream& input)
{
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

coposit::model::copositivity_mode parse_mode(const std::string& name)
{
    if (name == "copositive") return coposit::model::copositivity_mode::copositive;
    if (name == "strictly_copositive") return coposit::model::copositivity_mode::strictly_copositive;
    throw std::invalid_argument("mode must be 'copositive' or 'strictly_copositive'");
}

void print_usage(const char* program)
{
    std::cout << "Usage: " << program << " [--mode copositive|strictly_copositive] [FILE|-]\n"
                 "Read dimension#upper-triangle-values and print true or false; the default mode is strictly_copositive.\n";
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return 0;
    }

    try {
        coposit::model::copositivity_mode mode = coposit::model::copositivity_mode::strictly_copositive;
        int next_argument = 1;
        if (next_argument < argc && std::string(argv[next_argument]) == "--mode") {
            if (++next_argument == argc) throw std::invalid_argument("--mode requires a value");
            mode = parse_mode(argv[next_argument++]);
        }
        if (argc - next_argument > 1) throw std::invalid_argument("too many arguments");

        std::string input;
        if (next_argument < argc && std::string(argv[next_argument]) != "-") {
            std::ifstream file(argv[next_argument]);
            if (!file) throw std::runtime_error("cannot open input file");
            input = read_all(file);
        } else {
            input = read_all(std::cin);
        }

        const coposit::matrix_integer matrix = coposit::parse_integer_matrix(input);
        std::cout << (coposit::model::solve(matrix, mode) ? "true\n" : "false\n");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << argv[0] << ": " << error.what() << '\n';
        return 2;
    }
}
