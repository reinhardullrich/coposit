#include <gtest/gtest.h>
#include <coposit/model.hpp>
#include <coposit/open_node_limit.hpp>

#include "../source_trace.hpp"

using namespace coposit;

/*
 * Copositivity regression tests for exact-integer checker.
 *
 * Cases cover minimal dimensions and representative sign patterns where the
 * exact checker should clearly accept or reject.
 */

TEST(Dutour2018ModelTest, OneByOnePositive) {
    matrix_integer A(1, 1);
    A(0, 0) = integer(1);
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, OneByOneNegative) {
    matrix_integer A(1, 1);
    A(0, 0) = integer(-1);
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, OneByOneZero) {
    matrix_integer A(1, 1);
    A(0, 0) = integer(0);
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, TwoByTwoStrictlyCopositive) {
    matrix_integer A(2, 2);
    A(0, 0) = integer(2);
    A(0, 1) = integer(1);
    A(1, 0) = integer(1);
    A(1, 1) = integer(2);
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, TwoByTwoNotCopositive) {
    matrix_integer A(2, 2);
    A(0, 0) = integer(-1);
    A(0, 1) = integer(1);
    A(1, 0) = integer(1);
    A(1, 1) = integer(-1);
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, TwoByTwoPositiveDefinite) {
    matrix_integer A(2, 2);
    A(0, 0) = integer(2);
    A(0, 1) = integer(1);
    A(1, 0) = integer(1);
    A(1, 1) = integer(2);
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, ThreeByThreeStrictlyCopositive) {
    matrix_integer A;
    A.set_identity(3);
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, ThreeByThreeNotCopositive) {
    matrix_integer A(3, 3);
    A(0, 0) = integer(1); A(0, 1) = integer(0); A(0, 2) = integer(0);
    A(1, 0) = integer(0); A(1, 1) = integer(-1); A(1, 2) = integer(0);
    A(2, 0) = integer(0); A(2, 1) = integer(0); A(2, 2) = integer(1);
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, BeyondFormerDimensionLimitAcceptsIdentity) {
    matrix_integer A;
    A.set_identity(70);
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, RepeatedCallsAreDeterministic) {
    matrix_integer A;
    A.set_identity(3);
    bool result1 = model::solve(A);
    EXPECT_TRUE(result1);
    bool result2 = model::solve(A);
    EXPECT_TRUE(result2);
}

TEST(Dutour2018ModelTest, PositiveDefiniteTwoByTwo) {
    matrix_integer A(2, 2);
    A(0, 0) = integer(2); A(0, 1) = integer(1);
    A(1, 0) = integer(1); A(1, 1) = integer(2);
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, NegativeDiagonalTwoByTwo) {
    matrix_integer A(2, 2);
    A(0, 0) = integer(-1); A(0, 1) = integer(2);
    A(1, 0) = integer(2); A(1, 1) = integer(-1);
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, SingularTwoByTwo) {
    matrix_integer A(2, 2);
    A(0, 0) = integer(1);     A(0, 1) = integer(-1);
    A(1, 0) = integer(-1); A(1, 1) = integer(1);
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, FourByFourIdentityPasses) {
    matrix_integer A;
    A.set_identity(4);
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, FourByFourPartitionFindsNegativeDirection) {
    matrix_integer A(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) A(row, column) = integer(row == column ? 5 : -2);
    }
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, SourceTracePreservesSplitAndChildOrder) {
    matrix_integer A(2, 2);
    A(0, 0) = integer(2); A(0, 1) = integer(-1);
    A(1, 0) = integer(-1); A(1, 1) = integer(2);

    baseline_source_trace::clear();
    EXPECT_TRUE(model::solve(A));
    const std::vector<baseline_source_trace::event> expected{
        {"split", 0, 1}, {"push-second", 1, 0}, {"push-first", 0, 1}, {"accept"}, {"accept"}};
    EXPECT_EQ(baseline_source_trace::events, expected);
}

TEST(Dutour2018ModelTest, FourByFourPositiveOffDiagonalPasses) {
    matrix_integer A(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) A(row, column) = integer(row == column ? 1 : 2);
    }
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, FourByFourPartitionFindsZeroDirection) {
    matrix_integer A(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) A(row, column) = integer(row == column ? 3 : -1);
    }
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, FourByFourMixedSignKernelPasses) {
    constexpr slong v[4] = {1, -1, 1, -1};
    matrix_integer A(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            A(row, column) = integer((row == column ? 4 : 0) - v[row] * v[column]);
        }
    }
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, FourByFourAllOnesPasses) {
    matrix_integer A(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) A(row, column) = integer(1);
    }
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, ArbitraryPrecisionIntegerPartitionBranches) {
    integer big;
    ASSERT_EQ(fmpz_set_str(big.native_handle(), "123456789012345678901234567890", 10), 0);

    matrix_integer negative_determinant(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            negative_determinant(row, column) = big;
            negative_determinant(row, column).multiply(row == column ? 5 : 2);
            if (row != column) negative_determinant(row, column).negate();
        }
    }
    EXPECT_FALSE(model::solve(negative_determinant));

    matrix_integer singular(4, 4);
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            singular(row, column) = big;
            singular(row, column).multiply(row == column ? 3 : 1);
            if (row != column) singular(row, column).negate();
        }
    }
    EXPECT_FALSE(model::solve(singular));
}

TEST(Dutour2018ModelTest, AllZeros) {
    matrix_integer A(2, 2);
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, AllOnes) {
    matrix_integer A(2, 2);
    A(0, 0) = integer(1); A(0, 1) = integer(1);
    A(1, 0) = integer(1); A(1, 1) = integer(1);
    EXPECT_TRUE(model::solve(A));
}

TEST(Dutour2018ModelTest, BoundaryStressMatrix9161IsNotStrict) {
    constexpr slong upper_triangle[] = {1, -1, 1, 2, -3, 2, -3, -3, 4, 5, 6, -4, 5, -8, 16};
    matrix_integer A(5, 5);
    size_t next = 0;
    for (size_t row = 0; row < 5; ++row) {
        for (size_t column = row; column < 5; ++column) {
            A(row, column) = integer(upper_triangle[next++]);
            A(column, row) = A(row, column);
        }
    }
    EXPECT_FALSE(model::solve(A));
}

TEST(Dutour2018ModelTest, HildebrandCase34StopsAtTheOpenNodeLimit) {
    constexpr slong upper_triangle[] = {
        125, -150, -105, 585, -245, -825, 500, -450, -350, 1638, -770,
        1125, -1125, -735, 3861, 3125, -2625, -1925, 6125, -5775, 15125,
    };
    matrix_integer A(6, 6);
    size_t next = 0;
    for (size_t row = 0; row < 6; ++row) {
        for (size_t column = row; column < 6; ++column) {
            A(row, column) = integer(upper_triangle[next++]);
            A(column, row) = A(row, column);
        }
    }
    EXPECT_THROW(model::solve(A), open_node_limit_reached);
}

TEST(Dutour2018ModelTest, RejectsInvalidMatrixShapes) {
    matrix_integer empty;
    EXPECT_THROW(model::solve(empty), std::invalid_argument);

    matrix_integer non_square(2, 3);
    EXPECT_THROW(model::solve(non_square), std::invalid_argument);

    matrix_integer asymmetric(2, 2);
    asymmetric.set_identity(2);
    asymmetric(0, 1) = integer(1);
    EXPECT_THROW(model::solve(asymmetric), std::invalid_argument);
}

TEST(Dutour2018ModelTest, DecidesOrdinaryCopositivity) {
    constexpr auto ordinary = model::copositivity_mode::copositive;
    matrix_integer boundary(2, 2);
    boundary(0, 0) = integer(1); boundary(0, 1) = integer(-1);
    boundary(1, 0) = integer(-1); boundary(1, 1) = integer(1);
    EXPECT_TRUE(model::solve(boundary, ordinary));

    matrix_integer negative(boundary);
    negative(0, 1) = integer(-2); negative(1, 0) = integer(-2);
    EXPECT_FALSE(model::solve(negative, ordinary));

    matrix_integer zero(1, 1);
    EXPECT_TRUE(model::solve(zero, ordinary));
}
