#include <coposit/coposit.hpp>

#include <coposit/component_pipeline.hpp>
#include <coposit/model.hpp>

#include <stdexcept>

namespace {

void validate_matrix(const coposit::matrix_integer& matrix)
{
    if (matrix.rows() == 0 || matrix.cols() == 0) throw std::invalid_argument("matrix must be nonempty");
    if (matrix.rows() != matrix.cols()) throw std::invalid_argument("matrix must be square");
    for (size_t row = 0; row < matrix.rows(); ++row) {
        for (size_t column = row + 1; column < matrix.cols(); ++column) {
            if (matrix(row, column).compare(matrix(column, row)) != 0)
                throw std::invalid_argument("matrix must be symmetric");
        }
    }
}

} // namespace

namespace coposit {

copositivity_result check(const matrix_integer& matrix, copositivity_mode mode)
{
    validate_matrix(matrix);
    const component_pipeline::options preprocessing;
    switch (mode) {
    case copositivity_mode::both: {
        const model::copositivity_classification result = component_pipeline::classify(
            matrix, preprocessing, [](const matrix_integer& part) { return model::classify(part); });
        return {result.is_copositive, result.is_strictly_copositive};
    }
    case copositivity_mode::copositive: {
        constexpr model::copositivity_mode selected = model::copositivity_mode::copositive;
        const bool result = component_pipeline::check(
            matrix, selected, preprocessing, [selected](const matrix_integer& part) { return model::solve(part, selected); });
        return {result, std::nullopt};
    }
    case copositivity_mode::strictly_copositive: {
        constexpr model::copositivity_mode selected = model::copositivity_mode::strictly_copositive;
        const bool result = component_pipeline::check(
            matrix, selected, preprocessing, [selected](const matrix_integer& part) { return model::solve(part, selected); });
        return {std::nullopt, result};
    }
    }
    throw std::invalid_argument("invalid copositivity mode");
}

} // namespace coposit
