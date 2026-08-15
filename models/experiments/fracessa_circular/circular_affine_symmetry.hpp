#pragma once

#include <coposit/matrix_integer.hpp>
#include <coposit/support.hpp>
#include <coposit/timeout.hpp>

#include <cstddef>
#include <numeric>
#include <vector>

namespace coposit::model::fracessa_circular_detail {

/* Exact multiplier symmetries of a symmetric circulant matrix, after rotations and reflections are already identified. */
class circular_affine_symmetry {
public:
    explicit circular_affine_symmetry(const matrix_integer& matrix)
        : dimension_(matrix.rows())
        , transformed_(dimension_)
        , canonical_(dimension_)
    {
        destinations_.reserve((dimension_ + 1) / 2);
        source_positions_.reserve(dimension_);

        add_multiplier(1);
        for (size_t multiplier = 2; multiplier <= dimension_ / 2; ++multiplier) {
            if (std::gcd(multiplier, dimension_) != 1) continue;
            bool preserves_matrix = true;
            for (size_t offset = 0; offset < dimension_; ++offset) {
                if (matrix(0, offset).compare(matrix(0, (multiplier * offset) % dimension_)) != 0) {
                    preserves_matrix = false;
                    break;
                }
            }
            if (preserves_matrix) add_multiplier(multiplier);
        }

        images_.reserve(destinations_.size());
        for (size_t index = 0; index < destinations_.size(); ++index) images_.emplace_back(dimension_);
    }

    bool is_representative(const support& candidate) const
    {
        for (size_t multiplier_class = 1; multiplier_class < destinations_.size(); ++multiplier_class) {
            timeout_checkpoint();
            transform_into(candidate, multiplier_class, transformed_);
            for (size_t shift = 0; shift < dimension_; ++shift) {
                if (transformed_ < candidate) return false;
                transformed_.rotate_one_right();
            }

            transformed_.reflect();
            for (size_t shift = 0; shift < dimension_; ++shift) {
                if (transformed_ < candidate) return false;
                transformed_.rotate_one_right();
            }
        }
        return true;
    }

    template<class Callback>
    void for_each_distinct_bracelet_image(const support& candidate, Callback&& callback) const
    {
        size_t image_count = 0;
        for (size_t multiplier_class = 0; multiplier_class < destinations_.size(); ++multiplier_class) {
            timeout_checkpoint();
            canonical_dihedral(candidate, multiplier_class, canonical_);
            bool seen = false;
            for (size_t index = 0; index < image_count; ++index) {
                if (images_[index] == canonical_) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                images_[image_count] = canonical_;
                callback(images_[image_count++]);
            }
        }
    }

private:
    void add_multiplier(size_t multiplier)
    {
        destinations_.emplace_back(dimension_);
        for (size_t strategy = 0; strategy < dimension_; ++strategy) {
            destinations_.back()[strategy] = (multiplier * strategy) % dimension_;
        }
    }

    void transform_into(const support& candidate, size_t multiplier_class, support& image) const
    {
        image.clear();
        candidate.copy_indices_to(source_positions_);
        for (const size_t position : source_positions_) image.set(destinations_[multiplier_class][position]);
    }

    void canonical_dihedral(const support& candidate, size_t multiplier_class, support& smallest) const
    {
        transform_into(candidate, multiplier_class, smallest);
        transformed_ = smallest;
        for (size_t shift = 1; shift < dimension_; ++shift) {
            transformed_.rotate_one_right();
            if (transformed_ < smallest) smallest = transformed_;
        }

        transformed_.rotate_one_right();
        transformed_.reflect();
        for (size_t shift = 0; shift < dimension_; ++shift) {
            if (transformed_ < smallest) smallest = transformed_;
            transformed_.rotate_one_right();
        }
    }

    size_t dimension_;
    std::vector<std::vector<size_t>> destinations_;
    mutable std::vector<size_t> source_positions_;
    mutable support transformed_;
    mutable support canonical_;
    mutable std::vector<support> images_;
};

} // namespace coposit::model::fracessa_circular_detail
