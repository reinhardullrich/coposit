#pragma once

#include <coposit/matrix_integer.hpp>

#include <stdexcept>
#include <string_view>

namespace coposit::model {

enum class copositivity_mode { copositive, strictly_copositive };

struct copositivity_classification {
    bool is_copositive;
    bool is_strictly_copositive;
};

inline void require_strict_mode(copositivity_mode mode)
{
    if (mode != copositivity_mode::strictly_copositive) {
        throw std::invalid_argument("this model supports only strict copositivity");
    }
}

/* Each executable links exactly one model. The caller supplies a nonempty square symmetric matrix. */
bool solve(const matrix_integer& matrix, copositivity_mode mode = copositivity_mode::strictly_copositive);

/* Implemented only by models that can determine both predicates in one traversal. */
copositivity_classification classify(const matrix_integer& matrix);

/* Implemented only by models with one required runtime parameter. */
void configure(std::string_view parameter);

} // namespace coposit::model
