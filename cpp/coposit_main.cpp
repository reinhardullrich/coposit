#include "companion_launcher.hpp"

#include <coposit/incumbent.hpp>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char *models[] = {
#ifdef COPOSIT_BUILD_EXPERIMENTS
    "dutour_2018",
    "danninger_1990",
    "copomatrix_2011",
    "adaptive_dutour_danninger",
    "adaptive_dutour_copomatrix",
    "adaptive_sponsel_copomatrix",
    "adaptive_zischg_sponsel_copomatrix",
    "hadeler_1983",
    "dickinson_2019",
    "dense_bitset_dickinson",
    "interval_recursive_dickinson",
    "bdd_dickinson",
    "zdd_dickinson",
    "cbdd_dickinson",
    "cbdd_halfspace_dickinson",
    "upper_endpoint_cbdd_dickinson",
    "cbdd_dickinson_improved_1",
    "wide_certificate_cbdd_dickinson",
    "multithreaded_cbdd_dickinson",
    "ceiling_pruned_dickinson",
    "kernel_cone_dickinson",
    "affine_companion_dickinson",
    "layered_singular_lift_dickinson",
    "breadth_first_singular_lift_dickinson",
    "czdd_dickinson",
    "sat_dickinson",
    "sat_halfspace_dickinson",
    "sat_halfspace_rays_dickinson",
    "sat_b1",
    "sat_b2",
    "sat_b3",
    "clasp_b3",
    "bdd_b3",
    "sat_b4",
    "sat_b5",
    "nbc_b6",
    "nbc_b7",
    "improved_nbc_b7",
    "improved_nbc_x2",
    "improved_nbc_x3",
    "improved_nbc_x4",
    "improved_nbc_x5",
    "improved_nbc_x6",
    "improved_nbc_x7",
    "improved_nbc_x8",
    "interval_supports_g3",
    "minimal_sat_g4",
    "cadical_x1",
    "dual_frontier_nbc",
    "dual_frontier_nbc_two",
    "dual_frontier_nbc_three",
    "dual_frontier_nbc_four",
    "improved_nbc_b8",
    "improved_nbc_b9",
    "improved_nbc_g2",
    "sat_c1",
    "sat_c2",
    "sat_c3",
    "sat_c4",
    "f1",
    "f2",
    "g1",
    "milp_1",
    "sat_a1",
    "sat_a2",
    "sat_a3",
    "sat_a4",
    "sat_a5",
    "sat_halfspace_lp_dickinson",
    "sat_halfspace_milp_dickinson",
    "sat_halfspace_rays_lookahead_dickinson",
    "sat_halfspace_rays_wide_dickinson",
    "wide_certificate_sat_dickinson",
    "xxx",
    "xxx_two",
    "clingo_dickinson",
    "clingo_halfspace_dickinson",
    "support_pruned_dickinson",
    "nullity_support_pruned_dickinson",
    "rhs_dickinson",
    "frank_wolfe_dickinson",
    "one_step_frank_wolfe_dickinson",
    "pairwise_frank_wolfe_dickinson",
    "support_polished_frank_wolfe_dickinson",
    "safi_2021",
    "bundfuss_2008",
    "sponsel_2012",
    "frank_wolfe_sponsel",
    "fracessa",
    "zischg_hadeler",
    "zischg_dickinson",
    "zischg_fracessa",
#else
    coposit::incumbent_model.data(),
#endif
};

void print_usage(const char *program) {
  std::cout
      << "Usage: " << program
      << " [--model MODEL] [--mode strict|non-strict|both] [OPTIONS] "
         "[MATRIX|FILE|-]\n"
         "Options:\n"
         "  --preprocessing on|off\n"
         "  --diagnostics\n"
         "  --model-parameter VALUE\n"
         "  --timeout SECONDS\n"
         "  --version\n"
         "Omitting --model selects the incumbent " << coposit::incumbent_model << ".\n"
         "The fixed preprocessing pipeline is on by default. Combined-capable models default to mode both; other models require "
         "--mode.\n"
         "Models:\n";
  for (const char *model : models)
    std::cout << "  " << model << '\n';
}

bool known_model(const std::string &name) {
  return std::find(std::begin(models), std::end(models), name) !=
         std::end(models);
}

std::string companion_path(const char *launcher, const std::string &model) {
  const std::string launcher_path = launcher;
  const size_t separator = launcher_path.find_last_of("/\\");
  const std::string directory = separator == std::string::npos
                                    ? std::string{}
                                    : launcher_path.substr(0, separator + 1);
#ifdef _WIN32
  return directory + "coposit-" + model + ".exe";
#else
  return directory + "coposit-" + model;
#endif
}

int launch(const std::string &program, const std::string &display_name,
           int argc, char *argv[],
           const std::optional<coposit::cli::timeout_duration> &timeout) {
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
  return coposit::cli::launch_companion(program, std::move(arguments), timeout,
                                        display_name);
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc == 2 &&
      (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    print_usage(argv[0]);
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << COPOSIT_VERSION << '\n';
    return 0;
  }

  try {
    std::string model;
    std::optional<coposit::cli::timeout_duration> timeout;
    for (int index = 1; index < argc; ++index) {
      const std::string argument = argv[index];
      if (argument == "--model") {
        if (!model.empty())
          throw std::invalid_argument("--model may be given only once");
        if (++index == argc)
          throw std::invalid_argument("--model requires a value");
        model = argv[index];
      } else if (argument == "--timeout") {
        if (timeout)
          throw std::invalid_argument("--timeout may be given only once");
        if (++index == argc)
          throw std::invalid_argument("--timeout requires a value");
        timeout = coposit::cli::parse_timeout_seconds(argv[index]);
      }
    }
    const bool explicit_model = !model.empty();
    if (!explicit_model)
      model = std::string(coposit::incumbent_model);
    if (!known_model(model))
      throw std::invalid_argument("unknown model: " + model);
    return launch(companion_path(argv[0], model), explicit_model ? "coposit --model " + model : "coposit",
                  argc, argv, timeout);
  } catch (const std::exception &error) {
    std::cerr << argv[0] << ": " << error.what() << '\n';
    return 2;
  }
}
