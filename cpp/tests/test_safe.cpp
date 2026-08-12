#include <coposit/safe.hpp>

#include <gtest/gtest.h>

using namespace coposit;

TEST(SafeApiTest, DistinguishesStrictAndBoundaryMatrices)
{
    matrix_integer matrix;
    matrix.set_identity(2);
    EXPECT_TRUE(safe::is_strictly_copositive(matrix));

    matrix(0, 1) = integer(-1);
    matrix(1, 0) = integer(-1);
    EXPECT_FALSE(safe::is_strictly_copositive(matrix));
}
