#include <pybind11/pybind11.h>

#include <chrono>
#include <csignal>
#include <optional>
#include <stdexcept>
#include <string>

#include <signal.h>

#include <coposit/component_pipeline.hpp>
#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>
#include <coposit/parse_integer_matrix.hpp>
#include <coposit/timeout.hpp>

namespace py = pybind11;

namespace {

constexpr int kStatusOk = 0;
constexpr int kStatusParseError = 1;
constexpr int kStatusExecError = 4;
constexpr int kStatusTimeout = 5;
constexpr int kStatusNodeLimit = 6;
constexpr int kStatusInternalError = 255;

struct native_result {
    int status = kStatusInternalError;
    std::optional<bool> is_copositive;
    std::optional<bool> is_strictly_copositive;
    long long elapsed_ns = 0;
    std::string error_message;
};

coposit::model::copositivity_mode parse_mode(const std::string& name)
{
    if (name == "copositive") return coposit::model::copositivity_mode::copositive;
    if (name == "strictly_copositive") return coposit::model::copositivity_mode::strictly_copositive;
    throw std::invalid_argument("mode must be 'copositive' or 'strictly_copositive'");
}

coposit::component_pipeline::options parse_preprocessing(const std::string& name)
{
    coposit::component_pipeline::options selected;
    if (name == "none") {
        selected.pre_checks_enabled = false;
        selected.connected_components = false;
    } else if (name == "connected_components") {
        selected.pre_checks_enabled = false;
    } else if (name == "pre_checks") {
        selected.connected_components = false;
    } else if (name != "both") {
        throw std::invalid_argument("preprocessing must be 'none', 'connected_components', 'pre_checks', or 'both'");
    }
    return selected;
}

native_result compute_matrix_impl(const std::string& input, const std::string& mode_name, const std::string& preprocessing_name)
{
    coposit::matrix_integer matrix;
    try {
        matrix = coposit::parse_integer_matrix(input);
    } catch (const std::invalid_argument& error) {
        return {kStatusParseError, std::nullopt, std::nullopt, 0, error.what()};
    }

    const auto start = std::chrono::steady_clock::now();
    try {
        const coposit::component_pipeline::options preprocessing = parse_preprocessing(preprocessing_name);
#ifdef COPOSIT_HAS_COMBINED_CLASSIFICATION
        if (mode_name == "both") {
            const coposit::model::copositivity_classification classification = coposit::component_pipeline::classify(
                matrix, preprocessing, [](const coposit::matrix_integer& part) { return coposit::model::classify(part); });
            coposit::timeout_checkpoint();
            const auto end = std::chrono::steady_clock::now();
            return {kStatusOk, classification.is_copositive, classification.is_strictly_copositive,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), {}};
        }
#else
        if (mode_name == "both") throw std::invalid_argument("this model does not support combined classification");
#endif
        const coposit::model::copositivity_mode mode = parse_mode(mode_name);
        const bool classification = coposit::component_pipeline::check(
            matrix, mode, preprocessing,
            [&](const coposit::matrix_integer& part) { return coposit::model::solve(part, mode); });
        coposit::timeout_checkpoint();
        const auto end = std::chrono::steady_clock::now();
        return mode == coposit::model::copositivity_mode::copositive
            ? native_result{kStatusOk, classification, std::nullopt,
                            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), {}}
            : native_result{kStatusOk, std::nullopt, classification,
                            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), {}};
    } catch (const coposit::timeout_requested&) {
        const auto end = std::chrono::steady_clock::now();
        return {kStatusTimeout, std::nullopt, std::nullopt,
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), {}};
    } catch (const coposit::open_node_limit_reached& error) {
        const auto end = std::chrono::steady_clock::now();
        return {kStatusNodeLimit, std::nullopt, std::nullopt,
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), error.what()};
    } catch (const std::exception& error) {
        return {kStatusExecError, std::nullopt, std::nullopt, 0, error.what()};
    } catch (...) {
        return {kStatusInternalError, std::nullopt, std::nullopt, 0, "Unknown internal error"};
    }
}

void timeout_signal_handler(int) noexcept
{
    coposit::request_timeout();
}

void install_timeout_handler(int signal_number)
{
    struct sigaction action {};
    action.sa_handler = timeout_signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if (sigaction(signal_number, &action, nullptr) != 0) throw std::runtime_error("Could not install the timeout signal handler");
}

py::dict compute_matrix(const std::string& input, const std::string& mode_name, const std::string& preprocessing_name)
{
    native_result result;
    {
        py::gil_scoped_release release;
        result = compute_matrix_impl(input, mode_name, preprocessing_name);
        coposit::reset_timeout();
    }

    py::dict output;
    output["status"] = result.status;
    output["is_copositive"] = result.is_copositive ? py::cast(*result.is_copositive) : py::none();
    output["is_strictly_copositive"] = result.is_strictly_copositive ? py::cast(*result.is_strictly_copositive) : py::none();
    output["elapsed_ns"] = result.elapsed_ns;
    output["error_message"] = result.error_message;
    return output;
}

} // namespace

PYBIND11_MODULE(COPOSIT_PYTHON_MODULE, module)
{
    module.doc() = "Native copositivity model for pycoposit";
    module.attr("STATUS_OK") = kStatusOk;
    module.attr("STATUS_PARSE_ERROR") = kStatusParseError;
    module.attr("STATUS_EXEC_ERROR") = kStatusExecError;
    module.attr("STATUS_TIMEOUT") = kStatusTimeout;
    module.attr("STATUS_NODE_LIMIT") = kStatusNodeLimit;
    module.attr("STATUS_INTERNAL_ERROR") = kStatusInternalError;
    module.def("compute_matrix", &compute_matrix, py::arg("matrix"), py::arg("mode") = "strictly_copositive",
               py::arg("preprocessing") = "none");
    module.def("_install_timeout_handler", &install_timeout_handler, py::arg("signal_number"));
    module.def("_reset_timeout", &coposit::reset_timeout);
}
