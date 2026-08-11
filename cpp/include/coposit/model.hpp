#pragma once

#include <coposit/matrix_integer.hpp>

#include <stdexcept>

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

/* Each executable links exactly one self-contained model implementation of this function. */
bool solve(const matrix_integer& matrix, copositivity_mode mode = copositivity_mode::strictly_copositive);

/* Implemented only by models that can determine both predicates in one traversal. */
copositivity_classification classify(const matrix_integer& matrix);

} // namespace coposit::model
